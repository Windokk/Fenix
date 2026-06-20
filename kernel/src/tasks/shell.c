#include "tasks/shell.h"
#include "tasks/task.h"

#include "stdio.h"
#include "string.h"

#include "acpi/acpi.h"

#include "drivers/ps2/ps2_keyboard.h"
#include "drivers/video.h"
#include "drivers/rtc.h"

#include "graphics/gfx.h"

#include "memory/heap.h"

#include "profiling/profiling.h"

#include "cpu/smp.h"

#define INPUT_MAX    256
#define ARGS_MAX     16
#define HISTORY_SIZE 16

#define SC_UP   0x48
#define SC_DOWN 0x50

/* ------------------------------------------------------------------ */
/* History                                                              */
/* ------------------------------------------------------------------ */

static char history[HISTORY_SIZE][INPUT_MAX];
static int  history_count = 0; /* total entries stored, capped at HISTORY_SIZE */
static int  history_head  = 0; /* index where next entry will be written       */

static void history_push(const char *line) {
    if (line[0] == '\0') return;
    strncpy(history[history_head], line, INPUT_MAX - 1);
    history[history_head][INPUT_MAX - 1] = '\0';
    history_head = (history_head + 1) % HISTORY_SIZE;
    if (history_count < HISTORY_SIZE) history_count++;
}

/* pos: 0 = most recent, 1 = one before, etc. Returns NULL if out of range. */
static const char *history_get(int pos) {
    if (pos < 0 || pos >= history_count) return NULL;
    int idx = (history_head - 1 - pos + HISTORY_SIZE * 2) % HISTORY_SIZE;
    return history[idx];
}

/* ------------------------------------------------------------------ */
/* Line input                                                           */
/* ------------------------------------------------------------------ */

static int32_t char_widths[INPUT_MAX];

/* Erase the current line content from the display and reset len to 0. */
static void readline_erase_line(int *len) {
    for (int i = *len - 1; i >= 0; i--)
        console_erase_chars(char_widths[i]);
    *len = 0;
}

/* Print str into buf starting at position 0, updating char_widths and len. */
static void readline_load(char *buf, int max, int *len, const char *str) {
    int i = 0;
    while (str[i] && i < max - 1) {
        int32_t x_before = console_cursor_x();
        printf("%c", str[i]);
        char_widths[i] = console_cursor_x() - x_before;
        buf[i] = str[i];
        i++;
    }
    *len = i;
    buf[*len] = '\0';
}

static int shell_readline(char *buf, int max) {
    uint32_t cpu_id = cpu_self_id();
    cursor_show();
    int len          = 0;
    int history_pos  = -1; /* -1 = not browsing history */
    char saved[INPUT_MAX];  /* preserves the line being typed when UP is pressed */
    saved[0] = '\0';

    while (1) {
        key_event_t ev;
        while (!ps2_keyboard_get_event(&ev)) {
            if (cpu_local[cpu_id].current_task) {
                kb_waiter = cpu_local[cpu_id].current_task;
                task_block_local();
            }
        }

        if (ev.ascii == '\n') {
            cursor_hide();
            printf("\n");
            buf[len] = '\0';
            history_push(buf);
            return len;
        }

        if (ev.ascii == '\b') {
            if (len > 0) {
                len--;
                console_erase_chars(char_widths[len]);
            }
            continue;
        }

        /* Up arrow — go back in history */
        if (ev.scancode == SC_UP) {
            int next_pos = history_pos + 1;
            if (history_get(next_pos) == NULL) continue;
            if (history_pos == -1)
                strncpy(saved, buf, max); /* save current draft */
            readline_erase_line(&len);
            history_pos = next_pos;
            readline_load(buf, max, &len, history_get(history_pos));
            continue;
        }

        /* Down arrow — go forward in history (or restore draft) */
        if (ev.scancode == SC_DOWN) {
            if (history_pos == -1) continue;
            readline_erase_line(&len);
            history_pos--;
            if (history_pos == -1)
                readline_load(buf, max, &len, saved);
            else
                readline_load(buf, max, &len, history_get(history_pos));
            continue;
        }

        if (ev.ascii < 32 || ev.ascii > 126 || len >= max - 1)
            continue;

        /* Regular character — exit history browsing */
        history_pos = -1;

        int32_t x_before = console_cursor_x();
        printf("%c", ev.ascii);
        char_widths[len] = console_cursor_x() - x_before;
        buf[len++] = ev.ascii;
    }
}

/* ------------------------------------------------------------------ */
/* Argument parser                                                      */
/* ------------------------------------------------------------------ */

static char *argv[ARGS_MAX];
static char  arg_buf[INPUT_MAX];

static int shell_parse(const char *line, char **out_argv) {
    int argc = 0;
    int i    = 0;
    int j    = 0;

    while (line[i] == ' ') i++;   /* skip leading spaces */

    while (line[i] != '\0' && argc < ARGS_MAX - 1) {
        out_argv[argc++] = &arg_buf[j];
        while (line[i] != ' ' && line[i] != '\0')
            arg_buf[j++] = line[i++];
        arg_buf[j++] = '\0';
        while (line[i] == ' ') i++;
    }
    out_argv[argc] = NULL;
    return argc;
}

/* ------------------------------------------------------------------ */
/* Built-in commands                                                    */
/* ------------------------------------------------------------------ */

static void cmd_help(void) {
    printf("Available commands:\n");
    printf("  help         - show this message\n");
    printf("  clear        - clear the screen\n");
    printf("  echo <text>  - print text\n");
    printf("  time         - show current RTC time\n");
    printf("  mem          - show heap debug info\n");
    printf("  prof         - show profiling debug info\n");
    printf("  shutdown     - shut system down\n");
    printf("  reboot       - reboot system\n");
}

static void cmd_clear(void) {
    console_clear();
}

static void cmd_echo(int argc, char **argv) {
    for (int i = 1; i < argc; i++) {
        printf("%s", argv[i]);
        if (i < argc - 1) printf(" ");
    }
    printf("\n");
}

static void cmd_time(void) {
    rtc_time_t t = {0,0,0,0,0,0};
    rtc_read_time(&t);
    printf("%u/%u/%u %u:%u:%u\n",
           t.month, t.day, t.year + 2000,
           t.hours, t.minutes, t.seconds);
}

static void cmd_mem(void) {
    debug_heap_dump();
}

static void cmd_shutdown(void) {
    acpi_shutdown();
}

static void cmd_reboot(void) {
    acpi_reboot();
}

static void cmd_prof(void) {
    debug_prof_dump();
}

/* ------------------------------------------------------------------ */
/* Dispatch                                                           */
/* ------------------------------------------------------------------ */

static void shell_dispatch(const char *line) {
    if (line[0] == '\0') return;

    strncpy(arg_buf, line, INPUT_MAX - 1);
    arg_buf[INPUT_MAX - 1] = '\0';

    int argc = shell_parse(line, argv);
    if (argc == 0) return;

    if (strcmp(argv[0], "help")     == 0) { cmd_help();                   return; }
    if (strcmp(argv[0], "clear")    == 0) { cmd_clear();                  return; }
    if (strcmp(argv[0], "echo")     == 0) { cmd_echo(argc, argv);         return; }
    if (strcmp(argv[0], "time")     == 0) { cmd_time();                   return; }
    if (strcmp(argv[0], "mem")      == 0) { cmd_mem();                    return; }
    if (strcmp(argv[0], "prof")     == 0) { cmd_prof();                   return; }
    if (strcmp(argv[0], "shutdown") == 0) { cmd_shutdown();               return; }
    if (strcmp(argv[0], "reboot")   == 0) { cmd_reboot();                 return; }

    printf("%s: command not found\n", argv[0]);
}

/* ------------------------------------------------------------------ */
/* Shell task entry point                                               */
/* ------------------------------------------------------------------ */

void shell_task(void) {
    fb_set_color(255, 255, 255);

    static char input[INPUT_MAX];

    printf("Fenix Shell - type 'help' for commands\n\n");

    while (1) {
        printf("fenix> ");
        shell_readline(input, INPUT_MAX);
        shell_dispatch(input);
    }
}

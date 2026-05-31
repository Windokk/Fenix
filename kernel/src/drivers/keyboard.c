#include "drivers/keyboard.h"

#include "interrupts/pic.h"
#include "interrupts/idt.h"

#include "drivers/ps2.h"

#include "stdio.h"

// PS/2 Set 1 scancode => ASCII (unshifted / shifted)
// Index = scancode (0x00–0x58), value = [unshifted, shifted]

static const char scancode_map[2][89] = {
    /* unshifted */
    {
        0,    0,    '1',  '2',  '3',  '4',  '5',  '6',  /* 0x00–0x07 */
        '7',  '8',  '9',  '0',  '-',  '=',  '\b', '\t', /* 0x08–0x0F */
        'q',  'w',  'e',  'r',  't',  'y',  'u',  'i',  /* 0x10–0x17 */
        'o',  'p',  '[',  ']',  '\n', 0,    'a',  's',  /* 0x18–0x1F */
        'd',  'f',  'g',  'h',  'j',  'k',  'l',  ';',  /* 0x20–0x27 */
        '\'', '`',  0,    '\\', 'z',  'x',  'c',  'v',  /* 0x28–0x2F */
        'b',  'n',  'm',  ',',  '.',  '/',  0,    '*',  /* 0x30–0x37 */
        0,    ' ',  0,    0,    0,    0,    0,    0,    /* 0x38–0x3F */
        0,    0,    0,    0,    0,    0,    0,    '7',  /* 0x40–0x47 */
        '8',  '9',  '-',  '4',  '5',  '6',  '+',  '1',  /* 0x48–0x4F */
        '2',  '3',  '0',  '.',  0,    0,    0,    0,    /* 0x50–0x57 */
        0                                                  /* 0x58 */
    },
    /* shifted */
    {
        0,    0,    '!',  '@',  '#',  '$',  '%',  '^',  /* 0x00–0x07 */
        '&',  '*',  '(',  ')',  '_',  '+',  '\b', '\t', /* 0x08–0x0F */
        'Q',  'W',  'E',  'R',  'T',  'Y',  'U',  'I',  /* 0x10–0x17 */
        'O',  'P',  '{',  '}',  '\n', 0,    'A',  'S',  /* 0x18–0x1F */
        'D',  'F',  'G',  'H',  'J',  'K',  'L',  ':',  /* 0x20–0x27 */
        '"',  '~',  0,    '|',  'Z',  'X',  'C',  'V',  /* 0x28–0x2F */
        'B',  'N',  'M',  '<',  '>',  '?',  0,    '*',  /* 0x30–0x37 */
        0,    ' ',  0,    0,    0,    0,    0,    0,    /* 0x38–0x3F */
        0,    0,    0,    0,    0,    0,    0,    '7',  /* 0x40–0x47 */
        '8',  '9',  '-',  '4',  '5',  '6',  '+',  '1',  /* 0x48–0x4F */
        '2',  '3',  '0',  '.',  0,    0,    0,    0,    /* 0x50–0x57 */
        0                                                  /* 0x58 */
    }
};

// Modifier / special key scancodes (Set 1 make codes)
#define SC_LSHIFT   0x2A
#define SC_RSHIFT   0x36
#define SC_CTRL     0x1D
#define SC_ALT      0x38
#define SC_CAPSLOCK 0x3A

// Release codes = make code | 0x80
#define SC_RELEASE  0x80

// Modifier state
static bool shift_held   = false;
static bool ctrl_held    = false;
static bool alt_held     = false;
static bool caps_lock_on = false;

// Ring buffer
#define KB_BUF_SIZE 64

static key_event_t kb_buffer[KB_BUF_SIZE];
static uint8_t     kb_head    = 0;
static uint8_t     kb_tail    = 0;
task_t            *kb_waiter  = NULL;

static void kb_buf_push(key_event_t ev) {
    uint8_t next = (kb_head + 1) % KB_BUF_SIZE;
    if (next == kb_tail) return; /* full — drop oldest */
    kb_buffer[kb_head] = ev;
    kb_head = next;
}

static bool kb_buf_pop(key_event_t *out) {
    if (kb_head == kb_tail) return false;
    *out = kb_buffer[kb_tail];
    kb_tail = (kb_tail + 1) % KB_BUF_SIZE;
    return true;
}

// Public API
void keyboard_enable(void) {
    disable_interrupts();
    pic_unmask_irq(PIC_KEYBOARD_INTERRUPT);
    enable_interrupts();
    KLOG_INFO("PS/2","Keyboard enabled");
}

void keyboard_handle_irq(void) {
    uint8_t raw = inb(0x60);

    bool released = (raw & SC_RELEASE) != 0;
    uint8_t sc    = raw & ~SC_RELEASE;

    /* Update modifier state */
    if (sc == SC_LSHIFT || sc == SC_RSHIFT) {
        shift_held = !released;
    } else if (sc == SC_CTRL) {
        ctrl_held = !released;
    } else if (sc == SC_ALT) {
        alt_held = !released;
    } else if (sc == SC_CAPSLOCK && !released) {
        caps_lock_on = !caps_lock_on;
    }

    /* Determine ASCII value */
    char ascii = 0;
    if (sc < 89) {
        bool use_upper = shift_held ^ caps_lock_on;
        ascii = scancode_map[use_upper ? 1 : 0][sc];
    }

    key_event_t ev = {
        .scancode  = sc,
        .ascii     = ascii,
        .released  = released,
        .shift     = shift_held,
        .ctrl      = ctrl_held,
        .alt       = alt_held,
        .caps_lock = caps_lock_on,
    };

    if (!released) {
        kb_buf_push(ev);
        if (kb_waiter) {
            task_unblock(kb_waiter);
            kb_waiter = NULL;
        }
    }

    pic_send_eoi(PIC_KEYBOARD_INTERRUPT);
}

bool keyboard_has_input(void) {
    return kb_head != kb_tail;
}

bool keyboard_get_event(key_event_t *out) {
    return kb_buf_pop(out);
}

char keyboard_getchar(void) {
    key_event_t ev;
    while (!kb_buf_pop(&ev)) {
        if (current_task) {
            kb_waiter = current_task;
            task_block();
        }
    }
    return ev.ascii;
}

uint8_t keyboard_modifiers(void) {
    return (shift_held   ? KB_MOD_SHIFT : 0)
         | (ctrl_held    ? KB_MOD_CTRL  : 0)
         | (alt_held     ? KB_MOD_ALT   : 0)
         | (caps_lock_on ? KB_MOD_CAPS  : 0);
}

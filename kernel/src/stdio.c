#include "stdio.h"

#include "string.h"

#include "graphics/gfx.h"

#include "interrupts/idt.h"

#define CURSOR_W 8

static int32_t fb_cx = 0;
static int32_t fb_cy = 0;

static int32_t cursor_px, cursor_py;  // last-rendered position
static bool    cursor_on     = false;
static bool    cursor_active = false; // only true while readline is waiting for input

static uint8_t fb_color_r = 255;
static uint8_t fb_color_g = 255;
static uint8_t fb_color_b = 255;

spinlock_t fb_lock = {0};

static bool shell_line_active;
static const char *shell_line_prompt;
static char *shell_line_buf;
static int *shell_line_len;
static int32_t *shell_line_widths;
bool shell_line_erased;

void cursor_render(bool on){
    if (!video_info.mapped) return;
    if(on)
        gfx_draw_filled_rectangle(cursor_px, cursor_py + GFX_LINE_HEIGHT - 10, CURSOR_W, 2, 255, 255, 255);
    else
        gfx_draw_filled_rectangle(cursor_px, cursor_py + GFX_LINE_HEIGHT - 10, CURSOR_W, 2, 0, 0, 0);
}

void cursor_show(void) {
    uintptr_t _flags = disable_interrupts();
    lock(&fb_lock);
    cursor_active = true;
    cursor_render(false);
    cursor_px = fb_cx;
    cursor_py = fb_cy;
    cursor_render(true);
    cursor_on = true;
    unlock(&fb_lock);
    restore_interrupts(_flags);
}

void cursor_hide(void) {
    uintptr_t _flags = disable_interrupts();
    lock(&fb_lock);
    cursor_active = false;
    cursor_render(false);
    cursor_on = false;
    unlock(&fb_lock);
    restore_interrupts(_flags);
}

void cursor_sync(void) {
    if (!cursor_active) return;

    cursor_render(false);
    cursor_px = fb_cx;
    cursor_py = fb_cy;
    cursor_render(true);
    cursor_on = true;
}

void cursor_blink_tick(void) {
    uintptr_t _flags = disable_interrupts();
    lock(&fb_lock);

    if (!cursor_active) {
        unlock(&fb_lock);
        restore_interrupts(_flags);
        return;
    }

    cursor_on = !cursor_on;
    cursor_render(cursor_on);
    unlock(&fb_lock);
    restore_interrupts(_flags);
}

static void fb_scroll() {
    if (!video_info.mapped) return;
    uint32_t *fb = (uint32_t *)video_info.virt_addr;
    uint32_t  w  = video_info.width;
    uint32_t  h  = video_info.height;
    memmove(fb, fb + w * GFX_LINE_HEIGHT, w * (h - GFX_LINE_HEIGHT) * sizeof(uint32_t));
    memset(fb + w * (h - GFX_LINE_HEIGHT), 0, w * GFX_LINE_HEIGHT * sizeof(uint32_t));
    fb_cy = h - GFX_LINE_HEIGHT;
    cursor_sync();
}

int32_t console_cursor_x(void) {
    return fb_cx;
}

void console_clear(void) {
    uintptr_t _flags = disable_interrupts();
    lock(&fb_lock);
    if (!video_info.mapped) {
        unlock(&fb_lock);
        restore_interrupts(_flags);
        return;
    }
    uint32_t *fb = (uint32_t *)video_info.virt_addr;
    memset(fb, 0, video_info.width * video_info.height * sizeof(uint32_t));
    fb_cx = 0;
    fb_cy = 0;
    cursor_sync();

    unlock(&fb_lock);
    restore_interrupts(_flags);
}

void console_erase_chars(int32_t pixel_width) {
    uintptr_t _flags = disable_interrupts();
    lock(&fb_lock);

    if(shell_line_erased){
        redraw_shell_prompt();
        shell_line_erased = false;
        cursor_sync();
        unlock(&fb_lock);
        restore_interrupts(_flags);
        return;
    }

    if (!video_info.mapped || pixel_width <= 0) {
        unlock(&fb_lock);
        restore_interrupts(_flags);
        return;
    }
    fb_cx -= pixel_width;
    if (fb_cx < 0) fb_cx = 0;
    gfx_draw_filled_rectangle(fb_cx, fb_cy, pixel_width, GFX_LINE_HEIGHT, 0, 0, 0);
    cursor_sync();
    unlock(&fb_lock);
    restore_interrupts(_flags);
}

void fb_set_color(uint8_t r, uint8_t g, uint8_t b){
    fb_color_r = r;
    fb_color_g = g;
    fb_color_b = b;
}

static void fb_putchar(char c) {
    if (!video_info.mapped) return;
    if (c == '\n') {
        fb_cx = 0;
        fb_cy += GFX_LINE_HEIGHT;
        if (fb_cy + GFX_LINE_HEIGHT > (int32_t)video_info.height)
            fb_scroll();
        return;
    }
    fb_cx += gfx_draw_char(fb_cx, fb_cy, c, fb_color_r, fb_color_g, fb_color_b);
}

static void fb_puts(const char *s) {
    while (*s) fb_putchar(*s++);
}

void printf(const char* format, ...) {
	va_list args;
	va_start(args, format);
	vprintf(format, args);
	va_end(args);
}

int snprintf(char* buf, size_t n, const char *format, ...)
{
    va_list args;
	va_start(args, format);
	int ret = vsnprintf(buf, n, format, args);
	va_end(args);
    return ret;
}

char toupper(char c)
{
    if (c >= 'a' && c <= 'f') {
        return c - 32; // Convert lowercase letters to uppercase
    }
    return c;
}

void ftoa(float value, char* buffer, int precision) {
    int int_part = (int)value;
    float frac_part = value - int_part;

    // Convert integer part to string
    itoa(int_part, buffer, 10);
    
    // If precision > 0, handle fractional part
    if (precision > 0) {
        // Add decimal point
        char* ptr = buffer + strlen(buffer); // Go to end of the string
        *ptr++ = '.';

        // Multiply the fractional part by 10^precision
        for (int i = 0; i < precision; i++) {
            frac_part *= 10;
            int frac_digit = (int)frac_part;
            *ptr++ = '0' + frac_digit; // Convert to character
            frac_part -= frac_digit; // Remove the integer part
        }
        *ptr = '\0'; // Null-terminate the string
    }
}

void hex_to_ascii(uint64_t value, char* str, bool uppercase)
{
    const char *hex_digits = uppercase ? "0123456789ABCDEF" : "0123456789abcdef";
    char *p = str;
    int shift = 64 - 4;
    bool started = false;

    for (; shift >= 0; shift -= 4) {
        int digit = (value >> shift) & 0xF;
        if (digit != 0 || started) {
            *p++ = hex_digits[digit];
            started = true;
        }
    }

    if (!started) {
        *p++ = '0';
    }

    *p = '\0';
}

void klog_print(uint8_t r, uint8_t g, uint8_t b, const char* prefix, const char* fmt, ...) {
    uintptr_t _flags = disable_interrupts();
    lock(&fb_lock);
    va_list args;
	va_start(args, fmt);
    fb_color_r = r; fb_color_g = g; fb_color_b = b;
    if(shell_line_active){
        
        if(!shell_line_erased)
        {
            gfx_draw_filled_rectangle(0, fb_cy, fb_cx, GFX_LINE_HEIGHT, 0, 0, 0);
            fb_cx = 0;
            shell_line_erased = true;
        }
        
        // Hide cursor
        cursor_active = false;
        cursor_render(false);
        cursor_on = false;

        vprintf_unlocked(prefix, args);
        fb_color_r = 255; fb_color_g = 255; fb_color_b = 255;
        vprintf_unlocked(fmt, args);
	    va_end(args);

        cursor_active = true;
        cursor_render(false);
        cursor_px = fb_cx;
        cursor_py = fb_cy;
        cursor_render(true);
        cursor_on = true;

        unlock(&fb_lock);
        restore_interrupts(_flags);

        return;
    }
    vprintf_unlocked(prefix, args);
    fb_color_r = 255; fb_color_g = 255; fb_color_b = 255;
    vprintf_unlocked(fmt, args);
	va_end(args);
    unlock(&fb_lock);
    restore_interrupts(_flags);
}

void console_shell_line_begin(const char *prompt, char *buf, int *len, int32_t *widths)
{
    uintptr_t _flags = disable_interrupts();
    lock(&fb_lock);

    shell_line_active = true;
    shell_line_erased = false;

    shell_line_prompt = prompt;
    shell_line_buf = buf;
    shell_line_len = len;
    shell_line_widths = widths;

    unlock(&fb_lock);
    restore_interrupts(_flags);
}

void console_shell_line_end(void)
{
    uintptr_t _flags = disable_interrupts();
    lock(&fb_lock);

    shell_line_active = false;

    shell_line_prompt = NULL;
    shell_line_buf = NULL;
    shell_line_len = NULL;
    shell_line_widths = NULL;

    unlock(&fb_lock);
    restore_interrupts(_flags);
}

void console_shell_echo_char(char c, int32_t *width_out){
    uintptr_t _flags = disable_interrupts();
    lock(&fb_lock);
    if(shell_line_erased){

        redraw_shell_prompt();

        shell_line_erased = false;
    }
    int32_t fb_cx_save = fb_cx;
    fb_putchar(c);
    write_serial(c);
    *width_out = fb_cx - fb_cx_save;
    cursor_sync();
    unlock(&fb_lock);
    restore_interrupts(_flags);
}

void redraw_shell_prompt(void)
{
    int index = 0;
    while(shell_line_prompt[index] != '\0'){
        fb_putchar(shell_line_prompt[index]);
        write_serial(shell_line_prompt[index]);
        index++;
    }

    for(int i = 0; i < *shell_line_len; i++){
        int32_t fb_cx_save = fb_cx;
        fb_putchar(shell_line_buf[i]);
        write_serial(shell_line_buf[i]);
        shell_line_widths[i] = fb_cx - fb_cx_save;
    }
}

void vprintf(const char* format, va_list args) {
    uintptr_t _flags = disable_interrupts();
    lock(&fb_lock);
    uint8_t pad = 0;
    char buffer[64]; // Temporary buffer for numbers

    if(shell_line_active){

        if(!shell_line_erased)
        {
            gfx_draw_filled_rectangle(0, fb_cy, fb_cx, GFX_LINE_HEIGHT, 0, 0, 0);
            fb_cx = 0;
            shell_line_erased = true;
        }

        // Hide cursor
        cursor_active = false;
        cursor_render(false);
        cursor_on = false;

        

        vprintf_unlocked(format, args);

        cursor_active = true;
        cursor_render(false);
        cursor_px = fb_cx;
        cursor_py = fb_cy;
        cursor_render(true);
        cursor_on = true;

        unlock(&fb_lock);
        restore_interrupts(_flags);

        return;
    }

    while (*format != '\0') {
        if (*format == '%') {
            // Process length modifications
            uint8_t l_count = 0;
            format++;
            while(*format == 'l'){
                l_count++;
                format++;
            }
            // Process actual characters/format identifiers
            if (*format == '%') {
                write_serial('%');
                fb_putchar('%');
                format++;
            }
            else if (*format == 'c') {
                char cv = (char)va_arg(args, int);
                write_serial(cv);
                fb_putchar(cv);
                format++;
            } else if (*format == 's') {
                char *sv = (char*)va_arg(args, char*);
                if(sv == NULL){
                    sv = "";
                }
                serial_print(sv);
                fb_puts(sv);
                format++;
            } else if (*format == 'd' || *format == 'i') {
                /// @todo : support difference between long and long long
                if(l_count > 0)
                {
                    int64_t value = va_arg(args, int64_t);
                    if (value < 0) {
                        write_serial('-');
                        fb_putchar('-');
                        value = -value;
                    }
                    itoa(value, buffer, 10);
                }
                else{
                    int value = va_arg(args, int);
                    if (value < 0) {
                        write_serial('-');
                        fb_putchar('-');
                        value = -value;
                    }
                    itoa(value, buffer, 10);
                }
                serial_print(buffer);
                fb_puts(buffer);
                format++;
            } else if (*format == 'u') {
                /// @todo : support difference between long and long long
                if(l_count > 0){
                    uint64_t value = va_arg(args, uint64_t);
                    itoa(value, buffer, 10);
                }
                else{
                    unsigned int value = va_arg(args, unsigned int);
                    itoa(value, buffer, 10);
                }
                serial_print(buffer);
                fb_puts(buffer);
                format++;
            } else if (*format == 'x') {
                /// @todo : support difference between long and long long
                if(l_count > 0)
                {
                    uint64_t value = va_arg(args, uint64_t);
                    hex_to_ascii(value, buffer, false);
                    serial_print(buffer);
                    fb_puts(buffer);
                    format++;
                }
                else{
                    unsigned int value = va_arg(args, unsigned int);
                    hex_to_ascii(value, buffer, false);
                    serial_print(buffer);
                    fb_puts(buffer);
                    format++;
                }
            } else if (*format == 'X') {
                /// @todo : support difference between long and long long
                if(l_count > 0)
                {
                    uint64_t value = va_arg(args, uint64_t);
                    hex_to_ascii(value, buffer, true);
                    serial_print(buffer);
                    fb_puts(buffer);
                    format++;
                }
                else{
                    unsigned int value = va_arg(args, unsigned int);
                    hex_to_ascii(value, buffer, true);
                    serial_print(buffer);
                    fb_puts(buffer);
                    format++;
                }
            } else if (*format == 'f') {
                float value = va_arg(args, double);
                int precision = 6;
                ftoa(value, buffer, precision);
                serial_print(buffer);
                fb_puts(buffer);
                format++;
            } else if (*format >= '0' && *format <= '9') {
                if (*format == '0') {
                    format++;
                }
                while (*format >= '0' && *format <= '9') {
                    pad = pad * 10 + (*format++ - '0');
                }
            } else {
                write_serial('%');
                write_serial(*format);
                fb_putchar('%');
                fb_putchar(*format);
                format++;
            }
        } else {
            write_serial(*format);
            fb_putchar(*format);
            format++;
        }
    }
    cursor_sync();
    unlock(&fb_lock);
    restore_interrupts(_flags);
}

void vprintf_unlocked(const char *format, va_list args)
{
    uint8_t pad = 0;
    char buffer[64]; // Temporary buffer for numbers

    while (*format != '\0') {
        if (*format == '%') {
            // Process length modifications
            uint8_t l_count = 0;
            format++;
            while(*format == 'l'){
                l_count++;
                format++;
            }
            // Process actual characters/format identifiers
            if (*format == '%') {
                write_serial('%');
                fb_putchar('%');
                format++;
            }
            else if (*format == 'c') {
                char cv = (char)va_arg(args, int);
                write_serial(cv);
                fb_putchar(cv);
                format++;
            } else if (*format == 's') {
                char *sv = (char*)va_arg(args, char*);
                serial_print(sv);
                fb_puts(sv);
                format++;
            } else if (*format == 'd' || *format == 'i') {
                /// @todo : support difference between long and long long
                if(l_count > 0)
                {
                    int64_t value = va_arg(args, int64_t);
                    if (value < 0) {
                        write_serial('-');
                        fb_putchar('-');
                        value = -value;
                    }
                    itoa(value, buffer, 10);
                }
                else{
                    int value = va_arg(args, int);
                    if (value < 0) {
                        write_serial('-');
                        fb_putchar('-');
                        value = -value;
                    }
                    itoa(value, buffer, 10);
                }
                serial_print(buffer);
                fb_puts(buffer);
                format++;
            } else if (*format == 'u') {
                /// @todo : support difference between long and long long
                if(l_count > 0){
                    uint64_t value = va_arg(args, uint64_t);
                    itoa(value, buffer, 10);
                }
                else{
                    unsigned int value = va_arg(args, unsigned int);
                    itoa(value, buffer, 10);
                }
                serial_print(buffer);
                fb_puts(buffer);
                format++;
            } else if (*format == 'x') {
                /// @todo : support difference between long and long long
                if(l_count > 0)
                {
                    uint64_t value = va_arg(args, uint64_t);
                    hex_to_ascii(value, buffer, false);
                    serial_print(buffer);
                    fb_puts(buffer);
                    format++;
                }
                else{
                    unsigned int value = va_arg(args, unsigned int);
                    hex_to_ascii(value, buffer, false);
                    serial_print(buffer);
                    fb_puts(buffer);
                    format++;
                }
            } else if (*format == 'X') {
                /// @todo : support difference between long and long long
                if(l_count > 0)
                {
                    uint64_t value = va_arg(args, uint64_t);
                    hex_to_ascii(value, buffer, true);
                    serial_print(buffer);
                    fb_puts(buffer);
                    format++;
                }
                else{
                    unsigned int value = va_arg(args, unsigned int);
                    hex_to_ascii(value, buffer, true);
                    serial_print(buffer);
                    fb_puts(buffer);
                    format++;
                }
            } else if (*format == 'f') {
                float value = va_arg(args, double);
                int precision = 6;
                ftoa(value, buffer, precision);
                serial_print(buffer);
                fb_puts(buffer);
                format++;
            } else if (*format >= '0' && *format <= '9') {
                if (*format == '0') {
                    format++;
                }
                while (*format >= '0' && *format <= '9') {
                    pad = pad * 10 + (*format++ - '0');
                }
            } else {
                write_serial('%');
                write_serial(*format);
                fb_putchar('%');
                fb_putchar(*format);
                format++;
            }
        } else {
            write_serial(*format);
            fb_putchar(*format);
            format++;
        }
    }
    cursor_sync();
}

void buf_putchar(char * buf, size_t n, size_t* pos, char c){
    if(*pos < n-1)   
        buf[*pos] = c;
    (*pos)++;
}

int vsnprintf(char *buf, size_t n, const char *format, va_list args)
{
    if (n == 0) return 0;

    uint8_t pad = 0;
    char buffer[64]; // Temporary buffer for numbers

    size_t pos = 0;

    while (*format != '\0') {
        if (*format == '%') {
            // Process length modifications
            uint8_t l_count = 0;
            format++;
            while(*format == 'l'){
                l_count++;
                format++;
            }
            // Process actual characters/format identifiers
            if (*format == '%') {
                buf_putchar(buf, n, &pos, '%');
                format++;
            }
            else if (*format == 'c') {
                char cv = (char)va_arg(args, int);
                buf_putchar(buf, n, &pos, cv);
                format++;
            } else if (*format == 's') {
                char *sv = (char*)va_arg(args, char*);
                while (*sv) {
                    buf_putchar(buf, n, &pos, *sv++);
                }
                format++;
            } else if (*format == 'd' || *format == 'i') {
                /// @todo : support difference between long and long long
                if(l_count > 0)
                {
                    int64_t value = va_arg(args, int64_t);
                    if (value < 0) {
                        buf_putchar(buf, n, &pos, '-');
                        value = -value;
                    }
                    itoa(value, buffer, 10);
                }
                else{
                    int value = va_arg(args, int);
                    if (value < 0) {
                        buf_putchar(buf, n, &pos, '-');
                        value = -value;
                    }
                    itoa(value, buffer, 10);
                }
                char *p = buffer;
                while (*p) buf_putchar(buf, n, &pos, *p++);
                format++;
            } else if (*format == 'u') {
                /// @todo : support difference between long and long long
                if(l_count > 0){
                    uint64_t value = va_arg(args, uint64_t);
                    itoa(value, buffer, 10);
                }
                else{
                    unsigned int value = va_arg(args, unsigned int);
                    itoa(value, buffer, 10);
                }
                char *p = buffer;
                while (*p) buf_putchar(buf, n, &pos, *p++);
                format++;
            } else if (*format == 'x') {
                /// @todo : support difference between long and long long
                if(l_count > 0)
                {
                    uint64_t value = va_arg(args, uint64_t);
                    hex_to_ascii(value, buffer, false);
                    char *p = buffer;
                    while (*p) buf_putchar(buf, n, &pos, *p++);
                    format++;
                }
                else{
                    unsigned int value = va_arg(args, unsigned int);
                    hex_to_ascii(value, buffer, false);
                    char *p = buffer;
                    while (*p) buf_putchar(buf, n, &pos, *p++);
                    format++;
                }
            } else if (*format == 'X') {
                /// @todo : support difference between long and long long
                if(l_count > 0)
                {
                    uint64_t value = va_arg(args, uint64_t);
                    hex_to_ascii(value, buffer, true);
                    char *p = buffer;
                    while (*p) buf_putchar(buf, n, &pos, *p++);
                    format++;
                }
                else{
                    unsigned int value = va_arg(args, unsigned int);
                    hex_to_ascii(value, buffer, true);
                    char *p = buffer;
                    while (*p) buf_putchar(buf, n, &pos, *p++);
                    format++;
                }
            } else if (*format == 'f') {
                float value = va_arg(args, double);
                int precision = 6;
                ftoa(value, buffer, precision);
                char *p = buffer;
                while (*p) buf_putchar(buf, n, &pos, *p++); 
                format++;
            } else if (*format >= '0' && *format <= '9') {
                if (*format == '0') {
                    format++;
                }
                while (*format >= '0' && *format <= '9') {
                    pad = pad * 10 + (*format++ - '0');
                }
            } else {
                buf_putchar(buf, n, &pos, '%');
                buf_putchar(buf, n, &pos, *format);
                format++;
            }
        } else {
            buf_putchar(buf, n, &pos, *format);
            format++;
        }
    }

    if (n > 0) buf[pos < n ? pos : n - 1] = '\0';
    return (int)pos;
}
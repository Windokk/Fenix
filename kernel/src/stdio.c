#include "stdio.h"

#include "common.h"
#include "graphics/gfx.h"
#include "string.h"

#define CURSOR_W 8

static int32_t fb_cx = 0;
static int32_t fb_cy = 0;

static int32_t cursor_px, cursor_py;  // last-rendered position
static bool    cursor_on     = false;
static bool    cursor_active = false; // only true while readline is waiting for input

static uint8_t fb_color_r = 255;
static uint8_t fb_color_g = 255;
static uint8_t fb_color_b = 255;

void cursor_render(bool on){
    if (!video_info.mapped) return;
    if(on)
        gfx_draw_filled_rectangle(cursor_px, cursor_py + GFX_LINE_HEIGHT - 10, CURSOR_W, 2, 255, 255, 255);
    else
        gfx_draw_filled_rectangle(cursor_px, cursor_py + GFX_LINE_HEIGHT - 10, CURSOR_W, 2, 0, 0, 0);
}

void cursor_show(void) {
    cursor_active = true;
    cursor_render(false);
    cursor_px = fb_cx;
    cursor_py = fb_cy;
    cursor_render(true);
    cursor_on = true;
}

void cursor_hide(void) {
    cursor_active = false;
    cursor_render(false);
    cursor_on = false;
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
    if (!cursor_active) return;

    cursor_on = !cursor_on;
    cursor_render(cursor_on);
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
    if (!video_info.mapped) return;
    uint32_t *fb = (uint32_t *)video_info.virt_addr;
    memset(fb, 0, video_info.width * video_info.height * sizeof(uint32_t));
    fb_cx = 0;
    fb_cy = 0;
    cursor_sync();
}

void console_erase_chars(int32_t pixel_width) {
    if (!video_info.mapped || pixel_width <= 0) return;
    fb_cx -= pixel_width;
    if (fb_cx < 0) fb_cx = 0;
    gfx_draw_filled_rectangle(fb_cx, fb_cy, pixel_width, GFX_LINE_HEIGHT, 0, 0, 0);
    cursor_sync();
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

char toupper(char c)
{
    if (c >= 'a' && c <= 'f') {
        return c - 32; // Convert lowercase letters to uppercase
    }
    return c;
}

void to_hex(unsigned int value, char* buffer, bool uppercase) {
    const char* hex_digits = uppercase ? "0123456789ABCDEF" : "0123456789abcdef";
    int i = 0;

    // Handle 0 explicitly, because the loop won't run for value = 0
    if (value == 0) {
        buffer[i++] = '0';
    }

    // Convert the integer to hex
    while (value > 0) {
        buffer[i++] = hex_digits[value & 0xF];
        value >>= 4;
    }

    // Reverse the buffer to get the correct hex representation
    for (int j = 0; j < i / 2; ++j) {
        char temp = buffer[j];
        buffer[j] = buffer[i - j - 1];
        buffer[i - j - 1] = temp;
    }
    buffer[i] = '\0'; // Null-terminate the string
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

void vprintf(const char* format, va_list args) {
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
                unsigned int value = va_arg(args, unsigned int);
                to_hex(value, buffer, false);
                serial_print(buffer);
                fb_puts(buffer);
                format++;
            } else if (*format == 'X') {
                unsigned int value = va_arg(args, unsigned int);
                to_hex(value, buffer, true);
                serial_print(buffer);
                fb_puts(buffer);
                format++;
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
#pragma once

#include "common.h"
#include <stdarg.h>
#include <string.h>
#include "drivers/serial.h"

#include "cpu/smp.h"

void    printf(const char* format, ...);
int     snprintf(char* buf, size_t n, const char* format, ...);

char    toupper(char c);
void    ftoa(float value, char* buffer, int precision);

void    vprintf(const char* format, va_list args);
void    vprintf_unlocked(const char* format, va_list args);
int     vsnprintf(char * buf, size_t n, const char* format, va_list args);

int32_t console_cursor_x(void);
void    console_clear(void);
void    console_erase_chars(int32_t pixel_width);

void cursor_show(void);
void cursor_hide(void);
extern void cursor_blink_tick(void);

void fb_set_color(uint8_t r, uint8_t g, uint8_t b);

void hex_to_ascii(uint64_t value, char* str, bool uppercase);

void klog_print(uint8_t r, uint8_t g, uint8_t b, const char* prefix, const char* fmt, ...);



extern spinlock_t fb_lock;
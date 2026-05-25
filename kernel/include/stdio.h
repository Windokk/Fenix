#pragma once

#include "common.h"
#include <stdarg.h>
#include <string.h>
#include "drivers/serial.h"

void    printf(const char* format, ...);
char    toupper(char c);
void    to_hex(unsigned int value, char* buffer, bool uppercase);
void    ftoa(float value, char* buffer, int precision);
void    vprintf(const char* format, va_list args);

int32_t console_cursor_x(void);
void    console_clear(void);
void    console_erase_chars(int32_t pixel_width);

void cursor_show(void);
void cursor_hide(void);
extern void cursor_blink_tick(void);
#pragma once

#include "cdefs.h"
#include "stdarg.h"
#include "stdint.h"

#ifndef __STDIO_H
#define __STDIO_H

#ifdef __cplusplus
extern "C" {
#endif

enum std_key {
	KEY_SEQ = 0x1B,

	KEY_ESC = 0x100,

	KEY_F1, KEY_F2, KEY_F3, KEY_F4, KEY_F5, KEY_F6,
	KEY_F7, KEY_F8, KEY_F9, KEY_F10, KEY_F11, KEY_F12,

	KEY_PRINT, KEY_PAUSE,
	KEY_INSERT, KEY_HOME, KEY_PAGE_UP,
	KEY_DELETE, KEY_END, KEY_PAGE_DOWN,

	KEY_UP, KEY_LEFT, KEY_DOWN, KEY_RIGHT,
};

typedef uint64_t FILE;

#define EOF (-1)

#define stdout  ((FILE *) 1)
#define stdin   ((FILE *) 2)
#define stderr  ((FILE *) 3)


int printf(const char* __restrict str, ...);
int putchar(int i);
int puts(const char *str);
int sprintf(char* __restrict dest, const char* __restrict str, ...);
int vprintf(const char* __restrict str, va_list args);
int vsprintf(char* __restrict dest, const char* __restrict str, va_list args);

#ifdef __cplusplus
}
#endif

#endif
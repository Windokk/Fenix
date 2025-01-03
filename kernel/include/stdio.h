#ifndef __STDIO_H
#define __STDIO_H

#include "common.h"
#include <stdarg.h>
#include <string.h>
#include "drivers/serial.h"

void printf(const char* format, ...);
char toupper(char c);
void to_hex(unsigned int value, char* buffer, bool uppercase);
void vprintf(const char* format, va_list args);


#endif
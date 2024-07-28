#ifndef __STRING_H
#define __STRING_H

#include "cdefs.h"
#include "stddef.h"
#include "stdint.h"

#ifdef __cplusplus
extern "C" {
#endif
int memcmp(const void*, const void*, size_t);
void* memcpy(void*, const void*, uint64_t);
void* memmove(void*, const void*, uint64_t);
void* memset(void*, uint8_t, uint64_t);
char* strchr(const char*, char);
int strcmp(const char*, const char*);
char* strcpy(char*, const char*);
char* strdup(const char*);
uint64_t strlen(const char*);
int strncmp(const char*, const char*, uint64_t);
char* strncpy(char*, const char*, uint64_t);
char* strrchr(const char*, char);

#ifdef __cplusplus
}
#endif

#endif
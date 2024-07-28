#pragma once

#ifndef __STDLIB_H
#define __STDLIB_H

#ifdef __cplusplus
extern "C" {
#endif

_Noreturn void abort(void);

void* malloc(uint64_t);
void* calloc(uint64_t);
void* realloc(void*, uint64_t);
void free(void*);

char* realpath(const char*);

_Noreturn void exit(uint64_t);

void yield();


#ifdef __cplusplus
}
#endif

#endif
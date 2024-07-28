#pragma once

#ifndef __FENIX_SYSCALL_H
#define __FENIX_SYSCALL_H

#include "cdefs.h"
#include "stddef.h"
#include "stdio.h"
#include "stdint.h"

#define SYSCALL_KERNEL_ERR      1
#define SYSCALL_ERR             (-1)

#define SYSCALL_READ    0
#define SYSCALL_WRITE   1
#define SYSCALL_OPEN    2
#define SYSCALL_CLOSE   3
#define SYSCALL_EXIT    60

ssize_t syscall_write(const FILE *file, const void *buf, size_t count);

FILE* syscall_open(const char *filename, int flags, int mode);

int syscall_close(const FILE *stream);

_Noreturn void syscall_exit(int status);

#endif
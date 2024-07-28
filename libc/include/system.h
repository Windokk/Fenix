#pragma once

#ifndef __SYSTEM_H
#define __SYSTEM_H

#include "stdbool.h"
#include "stdint.h"

#ifdef __cplusplus
extern "C" {
#endif

enum syscall {
	SYS_YIELD, SYS_EXIT,
	SYS_EXEC, SYS_WAIT,
	SYS_GETCWD, SYS_CHDIR,

	SYS_MMAP, SYS_MUNMAP,

	SYS_DISK_READ, SYS_DISK_WRITE,
};

uint64_t syscall(enum syscall, ...);

static inline uint64_t exec(char* path, char** argv) {
	return syscall(SYS_EXEC, path, argv);
}

static inline uint64_t wait(uint64_t pid) {
	return syscall(SYS_WAIT, pid);
}

static inline char* getcwd(char* buffer) {
	return (char*) syscall(SYS_GETCWD, buffer);
}

static inline bool chdir(const char* path) {
	return (bool) syscall(SYS_CHDIR, path);
}

static inline void* mmap(void* vaddr, uint64_t size) {
	return (void*) syscall(SYS_MMAP, vaddr, size);
}

static inline void munmap(void* vaddr, uint64_t size) {
	syscall(SYS_MUNMAP, vaddr, size);
}

#ifdef __cplusplus
}
#endif

#endif
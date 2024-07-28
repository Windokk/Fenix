#pragma once

#ifndef __STDFILE_H
#define __STDFILE_H

#ifdef __cplusplus
extern "C" {
#endif

#include "stdbool.h"
#include "stdint.h"

#define STD_FILE_NAME_LENGTH 460
#define STD_FILE_ROOT_BLOCK 2048

#define STD_FILE_CREATE (1 << 0)
#define STD_FILE_CLEAR (1 << 1)

enum std_file_type {
	STD_FILE, STD_DIRECTORY
};

typedef union std_file {
	struct {
		uint64_t index;
		uint64_t parent;
		uint64_t child;
		uint64_t next;
		uint64_t size;
		uint64_t time;
		uint32_t type;
		char name[STD_FILE_NAME_LENGTH];
	} __attribute__ ((packed));
	struct {
		uint8_t boot_code[486];
		uint64_t total_blocks;
		uint64_t bitmap_blocks;
		uint64_t bitmap_offset;
		uint16_t boot_signature;
	} __attribute__ ((packed));
	uint64_t pointer[64];
} __attribute__ ((packed)) std_file_t;

std_file_t* std_file_open(const char*, uint64_t);
void std_file_close(std_file_t*);
bool std_file_parent(std_file_t*, std_file_t*);
bool std_file_child(std_file_t*, std_file_t*, const char*);
bool std_file_next(std_file_t*, std_file_t*);

bool std_file_add(std_file_t*, enum std_file_type, const char*);
bool std_file_remove(std_file_t*);

uint64_t std_file_write(std_file_t*, uint64_t, const void*, uint64_t);
uint64_t std_file_read(std_file_t*, uint64_t, void*, uint64_t);

#ifdef __cplusplus
}
#endif

#endif
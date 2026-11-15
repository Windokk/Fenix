#pragma once

#include "common.h"

#include "string.h"

#define FRAME_SIZE 0x1000        // 4KB
#define INDEX_FROM_BIT(a) (a/(8*4))

#define HEAP_SIZE (128 * 1024 * 1024)   // ~134 MB

extern uint64_t mmu_nframes; // Number of entries in bitset
extern uint32_t *mmu_frames; // Start address of bitset

extern uint64_t heap_start;
extern uint64_t heap_end;

extern void* kernel_end;

// Initializes the frame bitmap based on the total physical memory available
void init_frames(size_t total_memory);
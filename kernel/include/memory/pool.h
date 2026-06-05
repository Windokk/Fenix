#pragma once

#include "common.h"

typedef struct{
    void* free_head; // Pointer to first free slot in pool
    size_t slot_size; // Slot size, in bytes, aligned
    size_t capacity; // Nb slots in pool
    size_t free_count; // Nb free slots (currently) (should always validate : 0 <= free_count <= capacity)
} pool_t;

void pool_init(pool_t* pool, void* backing_memory, size_t total_bytes, size_t slot_size);

void* pool_alloc(pool_t* pool);

void pool_free(pool_t* pool, void* ptr);
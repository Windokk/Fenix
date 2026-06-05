#pragma once

#include "common.h"

#define ALIGN_UP(ptr, align) ((ptr + (uintptr_t)(align - 1)) & ~((uintptr_t)(align - 1)))

typedef struct slab_cache_t slab_cache_t;

typedef struct slab_t{
    struct               slab_t* prev;
    struct               slab_t* next;
    void*                free_head; // First free object ptr (for this slab)
    uint32_t             free_count; // Free objects (for this slab)
    uint32_t             obj_count; // Total nb of objects in this slab
    struct slab_cache_t* cache; // Parent cache pointer
} slab_t;

typedef struct slab_cache_t{

    char        name[32]; // debug
    size_t      obj_size; // Size of one object (in bytes)
    size_t      slot_size; // Size of one object (in bytes) (aligned)
    size_t      slab_header_size; // sizeof(slab_t) rounded to slot_size. Objects start pass this header.
    size_t      objs_per_slab; // (PAGE_SIZE - slab_header_size) / slot_size
    slab_t*     partial; // Partially filled slabs
    slab_t*     full; // Fully filled slabs
    slab_t*     empty; // Completely empty slabs

} slab_cache_t;

slab_cache_t* slab_cache_create(const char* name, size_t obj_size);
void*         slab_cache_alloc(slab_cache_t* cache);
void          slab_cache_free(void* ptr);
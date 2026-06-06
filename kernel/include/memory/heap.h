#pragma once

#include "common.h"

#define PAGE_PRESENT_CPL0 0x03

#define HEAP_WALKER(name) for(mblock_t *name = root_mblock; name != NULL; name = name->next) 

#define MBLOCK_MAGIC 0xDEAD 

struct mblock_struct {
  uint32_t  size;  
  uint16_t  magic;
  bool  free;
  struct mblock_struct*  next;
};
typedef struct mblock_struct mblock_t;

void init_heap();
void debug_heap_dump();
void *kmalloc(uint32_t size);
void *kmalloc_aligned(uint32_t size, uint32_t align);
void *kcalloc(size_t num, size_t size);
void kfree(void* ptr);
void kfree_aligned(void *ptr);
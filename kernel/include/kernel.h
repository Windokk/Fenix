#pragma once

#include "common.h"

extern void HALT_AND_CATCH_FIRE() __attribute__((noreturn));

static inline void x86_hlt(void) {
    __asm__ __volatile__ ("hlt"); 
}

typedef struct memory_infos{
  uint64_t memory_map_address;
  uint64_t total_memory_size;
  uint64_t usable_start;
  uint64_t usable_size;
} memory_infos_t;
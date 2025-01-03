#ifndef __KERNEL_H
#define __KERNEL_H

#include "drivers/serial.h"
#include "drivers/vesa.h"

void HALT_AND_CATCH_FIRE(const char *fmt, ...);

static inline void x86_hlt(void) {
    __asm__ __volatile__ ("hlt"); 
}

typedef struct memory_infos{
  uint64_t memory_map_address;
  uint64_t total_memory_size;
  uint64_t usable_start;
  uint64_t usable_size;
} memory_infos_t;

#endif // _KERNEL_H
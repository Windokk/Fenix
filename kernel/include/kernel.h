#ifndef __KERNEL_H
#define __KERNEL_H

#include "drivers/serial.h"



#define KERNEL_START_MEMORY 0xFFFF800000000000
#define KERNEL_VMA ((char*)KERNEL_START_MEMORY)

#define KERNEL_USER_VIEW_START_MEMORY 0xFFFFE00000000000
#define KERNEL_USER_VIEW_VMA ((char*)KERNEL_USER_VIEW_START_MEMORY)

#define KERNEL_VIDEO_MEMORY ((unsigned char*)0xFFFFC00000000000)

#define TO_VMA_U64(ptr) ((uint64_t)ptr + (uint64_t)KERNEL_START_MEMORY)
#define TO_VMA_PTR(type, ptr) ((type)TO_VMA_U64(ptr))
#define TO_PHYS_U64(ptr) ((uint64_t)ptr ^ (uint64_t)KERNEL_START_MEMORY)
#define TO_PHYS_PTR(type, ptr) ((type)TO_PHYS_U64(ptr))

#define GDT_NULL        0x00
#define GDT_KERNEL_CODE 0x08
#define GDT_KERNEL_DATA 0x10
#define GDT_USER_CODE   0x18
#define GDT_USER_DATA   0x20
#define GDT_TSS         0x28

void HALT_AND_CATCH_FIRE(const char *fmt, ...);

#define DEBUG debug_print

#define __UNUSED__ __attribute__((unused))

static inline void x86_hlt(void) {
    __asm__ __volatile__ ("hlt"); 
}

static inline uint64_t x86_get_cr3(void) {
    uint64_t rv;

    __asm__ __volatile__ (
        "mov %%cr3, %0"
        : "=r" (rv));
    return rv;
}
static inline void x86_set_cr3(uint64_t cr3)
{
  asm volatile (
    "mov %0, %%cr3"
    :
    : "a"(cr3)
    : "memory"
  );
}

static inline void x86_tlb_flush_all(void) {
  x86_set_cr3(x86_get_cr3());
}

#endif // _KERNEL_H
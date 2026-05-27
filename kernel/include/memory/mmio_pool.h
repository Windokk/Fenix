#pragma once
#include "common.h"

/* Constants duplicated from paging_pages.h — keep in sync if those change.
 * This header exists so that uacpi_impl.c can call paging_map/unmap_mmio()
 * without pulling in the static page-table arrays from paging_pages.h, which
 * would add ~136 KB of duplicate BSS per translation unit. */

#ifndef PAGE_SIZE
#define PAGE_SIZE 0x1000
#endif

#ifndef KERNEL_START_MEMORY
#define KERNEL_START_MEMORY 0xFFFF800000000000ULL
#endif

#ifndef TO_VMA_U64
#define TO_VMA_U64(ptr) ((uint64_t)(ptr) + KERNEL_START_MEMORY)
#endif

#ifndef ACPI_TABLE_BASE
#define ACPI_TABLE_BASE   0xFFFFE80000000000ULL
#endif
#ifndef ACPI_TABLE_WINDOW
#define ACPI_TABLE_WINDOW (16ULL << 20)
#endif

#ifndef MMIO_POOL_BASE
#define MMIO_POOL_BASE    0xFFFFE40000000000ULL
#endif
#ifndef MMIO_POOL_PAGES
#define MMIO_POOL_PAGES   512
#endif

void *paging_map_mmio(uint64_t phys, uint64_t size);
void  paging_unmap_mmio(void *virt, uint64_t size);

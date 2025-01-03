#pragma once

#ifndef __PAGING_PAGES_H
#define __PAGING_PAGES_H

#include "memory/paging_frames.h"
#include "memory/heap.h"

#include "drivers/vesa.h"

#include "stdio.h"

#define PAGE_SIZE 0x1000       // 4KB



#define KERNEL_START_MEMORY 0xFFFF800000000000
#define KERNEL_VMA ((char*)KERNEL_START_MEMORY)

#define TO_VMA_U64(ptr) ((uint64_t)(ptr) - (uint64_t)KERNEL_START_MEMORY)
#define TO_VMA_PTR(type, ptr) ((type)TO_VMA_U64(ptr))
#define TO_PHYS_U64(ptr) ((uint64_t)ptr ^ (uint64_t)KERNEL_START_MEMORY)
#define TO_PHYS_PTR(type, ptr) ((type)TO_PHYS_U64(ptr))

#define GDT_NULL        0x00
#define GDT_KERNEL_CODE 0x08
#define GDT_KERNEL_DATA 0x10
#define GDT_USER_CODE   0x18
#define GDT_USER_DATA   0x20
#define GDT_TSS         0x28



// PML4 (Page Map Level 4) entry structure
typedef union pml4e_struct {
    uint64_t all;
    struct {
        uint64_t present         : 1;   // Page present in memory
        uint64_t writable        : 1;   // Read-only if clear, read-write if set
        uint64_t user            : 1;   // Supervisor level only if clear
        uint64_t write_through   : 1;   // Write-through flag
        uint64_t disable_cache   : 1;   // Disable cache 
        uint64_t accessed        : 1;   // Has the page been accessed since last refresh?
        uint64_t ignore_1        : 1;   // Ignored
        uint64_t mbz_1           : 1;   // Must be zero
        uint64_t mbz_2           : 1;   // Must be zero
        uint64_t avail_1         : 3;   // Available for OS to use
        uint64_t address         : 40;  // Frame address (shifted right 12 bits)
        uint64_t avail_2         : 11;  // Available for OS to use
        uint64_t no_exec         : 1;   // No execute flag (NX bit)
    } fields;
} __attribute__((packed)) pml4e_t;

// PDPT (Page Directory Pointer Table) entry structure
typedef union pdpe_struct {
    uint64_t all;
    struct {
        uint64_t present         : 1;   // Page present in memory
        uint64_t writable        : 1;   // Read-only if clear, read-write if set
        uint64_t user            : 1;   // Supervisor level only if clear
        uint64_t write_through   : 1;   // Write-through flag
        uint64_t disable_cache   : 1;   // Disable cache 
        uint64_t accessed        : 1;   // Has the page been accessed since last refresh?
        uint64_t ignore_1        : 1;   // Ignored
        uint64_t mbz_1           : 1;   // Must be zero
        uint64_t ignore_2        : 1;   // Ignored
        uint64_t avail_1         : 3;   // Available for OS to use
        uint64_t address         : 40;  // Frame address (shifted right 12 bits)
        uint64_t avail_2         : 11;  // Available for OS to use
        uint64_t no_exec         : 1;   // No execute flag (NX bit)
    } fields;
} __attribute__((packed)) pdpe_t;

// PDE (Page Directory Entry) structure
typedef union pde_struct {
    uint64_t all;
    struct {
        uint64_t present         : 1;   // Page present in memory
        uint64_t writable        : 1;   // Read-only if clear, read-write if set
        uint64_t user            : 1;   // Supervisor level only if clear
        uint64_t write_through   : 1;   // Write-through flag
        uint64_t disable_cache   : 1;   // Disable cache 
        uint64_t accessed        : 1;   // Has the page been accessed since last refresh?
        uint64_t dirty           : 1;   // Page is dirty
        uint64_t mbo_1           : 1;   // Must be 1
        uint64_t global          : 1;   // Global
        uint64_t avail_1         : 3;   // Available for OS to use
        uint64_t pat             : 1;   // Page attribute table
        uint64_t mbz_1           : 8;   // Must be zero
        uint64_t address         : 31;  // Frame address (shifted right 12 bits)
        uint64_t avail_2         : 11;  // Available for OS to use
        uint64_t no_exec         : 1;   // No execute flag (NX bit)
    } fields;
} __attribute__((packed)) pde_t;

// PTE (Page Table Entry) structure
typedef union pte_struct {
    uint64_t all;
    struct {
        uint64_t present         : 1;   // Page present in memory
        uint64_t writable        : 1;   // Read-only if clear, read-write if set
        uint64_t user            : 1;   // Supervisor level only if clear
        uint64_t write_through   : 1;   // Write-through flag
        uint64_t disable_cache   : 1;   // Disable cache 
        uint64_t accessed        : 1;   // Has the page been accessed since last refresh?
        uint64_t dirty           : 1;   // Page is dirty
        uint64_t global          : 1;   // Global page
        uint64_t avail_1         : 3;   // Available for OS to use
        uint64_t address         : 40;  // Frame address (shifted right 12 bits)
        uint64_t avail_2         : 11;  // Available for OS to use
        uint64_t no_exec         : 1;   // No execute flag (NX bit)
    } fields;
} __attribute__((packed)) pte_t;

static pml4e_t pml4e[512] __attribute__((aligned(4096)));
static pdpe_t  pdpe[512] __attribute__((aligned(4096)));
static pde_t   pde[512] __attribute__((aligned(4096)));
static pte_t   pte[512] __attribute__((aligned(4096)));

static pdpe_t  pdpe_user[512] __attribute__((aligned(4096)));
static pde_t   pde_user[512] __attribute__((aligned(4096)));
static pte_t   pte_user[512] __attribute__((aligned(4096)));

static pdpe_t  pdpe_krnluser[512] __attribute__((aligned(4096)));
static pdpe_t  pde_krnluser[512] __attribute__((aligned(4096)));
static pte_t   pte_krnluser[512] __attribute__((aligned(4096)));

static pdpe_t  pdpe_video[512] __attribute__((aligned(4096)));
static pde_t   pde_video[512] __attribute__((aligned(4096)));
static pte_t   pte_video[512] __attribute__((aligned(4096)));




static void init_paging_kernel();

static void init_paging_user();

static void init_paging_video();

void init_paging();

#endif
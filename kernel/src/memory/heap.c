#include "memory/heap.h"

#include "kernel.h"

#include "memory/paging_frames.h"

#include "interrupts/idt.h"

#include "stdio.h"

#include "profiling/profiling.h"

mblock_t *root_mblock;

static uint32_t align_to(uint32_t addr, uint32_t align) {
    return (addr + align - 1) & ~(align - 1);
}

static mblock_t *find_heap_block(uint32_t size) {
    HEAP_WALKER(mb) {
        // exact fit now allowed
        if (mb->free && mb->size >= size) {
            return mb;
        }
    }

    return NULL;
}

static mblock_t *split_heap_block(mblock_t *mb, uint32_t size) {
    // If remaining space is too small,
    // don't split the block.
    if (mb->size <= size + sizeof(mblock_t)) {
        mb->free = false;
        return mb;
    }

    uint32_t old_size = mb->size;
    mblock_t *old_next = mb->next;

    uint8_t *ptr = (uint8_t*)mb;

    // current allocated block
    mb->size = size;
    mb->free = false;

    // new free block
    mblock_t *next_mb =
        (mblock_t*)(ptr + sizeof(mblock_t) + size);

    next_mb->magic = MBLOCK_MAGIC;
    next_mb->size = old_size - size - sizeof(mblock_t);
    next_mb->free = true;
    next_mb->next = old_next;

    mb->next = next_mb;

    return mb;
}

void *kmalloc(uint32_t size) {

    #ifdef KERNEL_PROFILING
    uint64_t t0 = rdtsc();
    #endif

    if (size == 0) {
        return NULL;
    }

    uintptr_t int_state = disable_interrupts();

    // align to 8-byte boundary
    size = align_to(size, 8);

    mblock_t *mb = find_heap_block(size);

    if (mb == NULL) {
        // TODO:
        // expand heap here with paging/sbrk
        // instead of infinite recursion

        KLOG_ERROR("HEAP", "Out of memory allocating %u bytes\n", size);
        restore_interrupts(int_state);
        return NULL;
    }

    mb = split_heap_block(mb, size);

    restore_interrupts(int_state);

    #ifdef KERNEL_PROFILING
    prof_record(PROF_KMALLOC, rdtsc() - t0);
    #endif

    return ((uint8_t*)mb) + sizeof(mblock_t);
}

void *kmalloc_aligned(uint32_t size, uint32_t align)
{
    void* ptr = kmalloc(size + align - 1 + sizeof(void*));

    void* aligned_ptr = (void*)(((uintptr_t)ptr + sizeof(void*) + (align - 1)) & ~((uintptr_t)(align - 1)));

    *(void**)(aligned_ptr - sizeof(void*)) = ptr;

    return aligned_ptr;
}

void kfree_aligned(void *ptr){
    void* original_ptr = *(void**)((uint8_t*)ptr - sizeof(void*));
    kfree(original_ptr);
}

void *kcalloc(size_t num, size_t size)
{
    if (num == 0 || size == 0) {
        return NULL;
    }

    uintptr_t int_state = disable_interrupts();

    uint32_t total = num * size;

    void *ptr = kmalloc(total);

    if (ptr == NULL) {
        return NULL;
    }

    memset(ptr, 0, total);

    restore_interrupts(int_state);

    return ptr;
}

static void coalesce_free_blocks() {
    mblock_t *mb = root_mblock;

    while (mb && mb->next) {
        if (mb->free && mb->next->free) {
            // merge adjacent free blocks
            mb->size += sizeof(mblock_t) + mb->next->size;
            mb->next = mb->next->next;

            // stay on same block
            continue;
        }

        mb = mb->next;
    }
}

void kfree(void *ptr) {

    #ifdef KERNEL_PROFILING
    uint64_t t0 = rdtsc();
    #endif

    if (ptr == NULL) {
        return;
    }

    uintptr_t int_state = disable_interrupts();

    mblock_t *mb =
        (mblock_t*)(((uint8_t*)ptr) - sizeof(mblock_t));

    if (mb->magic != MBLOCK_MAGIC) {
        KLOG_FATAL(
            "HEAP","free(%p) - invalid block\n",
            ptr
        );
    }

    if (mb->free) {
        KLOG_FATAL(
            "HEAP"," double free detected (%p)\n",
            ptr
        );
    }

    mb->free = true;

    // reduce fragmentation
    coalesce_free_blocks();

    restore_interrupts(int_state);

    #ifdef KERNEL_PROFILING
    prof_record(PROF_KFREE, rdtsc() - t0);
    #endif

}

void debug_heap_dump()
{
    uint32_t total = 0, used = 0, free_bytes = 0, blocks = 0;

    HEAP_WALKER(mb) {
        blocks++;
        total += mb->size;
        if (mb->free) free_bytes += mb->size;
        else           used      += mb->size;
    }

    printf("--- Heap dump (%u blocks) ---\n", blocks);
    printf("  total  : %u bytes\n", total);
    printf("  used   : %u bytes\n", used);
    printf("  free   : %u bytes\n", free_bytes);

    /*
    uint32_t i = 0;
    HEAP_WALKER(mb) {
        KLOG_INFO(
            "HEAP","[%u] %s  size=%u  addr=0x%X",
               i++,
               mb->free ? "FREE" : "USED",
               mb->size,
               (uint32_t)(uint64_t)mb);
    }*/
}

void init_heap()
{
    root_mblock = (mblock_t*)heap_start;

    root_mblock->magic = MBLOCK_MAGIC;
    root_mblock->free = true;
    root_mblock->next = NULL;

    root_mblock->size =
        heap_end - heap_start - sizeof(mblock_t);

    KLOG_INFO(
        "HEAP","Init start=0x%X end=0x%X size=%u",
        heap_start,
        heap_end,
        root_mblock->size
    );
}
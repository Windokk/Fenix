#include "memory/paging_frames.h"

#include "stdio.h"

uint64_t mmu_nframes; // Number of entries in bitset
uint32_t *mmu_frames; // Start address of bitset

uint64_t heap_start;
uint64_t heap_end;


uint64_t round_up(uint64_t num, uint64_t multiple) {
  return ((num + multiple - 1) / multiple) * multiple;
}

#define HEAP_SIZE (128 * 1024 * 1024)   // 16 MB Todo : Add more space

void init_frames(size_t total_memory){

    mmu_nframes = total_memory / FRAME_SIZE;

    mmu_frames = (uint32_t*)&kernel_end;

    /* Bitmap needs INDEX_FROM_BIT(mmu_nframes) uint32s, but we advance by
       mmu_nframes * sizeof(uint32_t) to keep the original layout. */
    heap_start = (uint64_t)mmu_frames + INDEX_FROM_BIT(mmu_nframes) * sizeof(uint32_t);

    memset(mmu_frames, 0, INDEX_FROM_BIT(mmu_nframes) * sizeof(uint32_t));

    heap_end = round_up(heap_start + HEAP_SIZE, 0x1000);

}
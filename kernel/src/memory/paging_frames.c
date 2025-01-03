#include "memory/paging_frames.h"

uint64_t mmu_nframes; // Number of entries in bitset
uint32_t *mmu_frames; // Start address of bitset

uint64_t heap_start;
uint64_t heap_end;


uint64_t round_up(uint64_t num, uint64_t multiple) {
  return ((num + multiple - 1) / multiple) * multiple;
}

void init_frames(size_t total_memory){

    // Calculate the total number of frames
    mmu_nframes = total_memory / FRAME_SIZE;

    // Define the memory location for frames (assumed to be after kernel_end)
    mmu_frames = (uint32_t*)&kernel_end;

    // Calculate the heap start address after frames
    heap_start = (uint64_t)mmu_frames + mmu_nframes * sizeof(uint32_t);

    // Clear the frames memory (assumed)
    memset(mmu_frames, 0, INDEX_FROM_BIT(mmu_nframes) * sizeof(uint32_t));

    // Calculate the heap end address
    heap_end = round_up(heap_start, 0x1000);

}
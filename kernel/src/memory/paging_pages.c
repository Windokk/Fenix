#include "memory/paging_pages.h"

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


static void init_paging_kernel() {

    memset(&pml4e, 0, sizeof(pml4e_t) * 512);
    memset(&pdpe, 0, sizeof(pdpe_t) * 512);
    memset(&pde, 0, sizeof(pde_t) * 512);
    memset(&pte, 0, sizeof(pte_t) * 512);
    memset(&pdpe_krnluser, 0, sizeof(pdpe_t) * 512);
    memset(&pde_krnluser, 0, sizeof(pde_t) * 512);
    memset(&pte_krnluser, 0, sizeof(pte_t) * 512);

    pml4e[0].all = TO_PHYS_U64(&pdpe) | 3;
    pml4e[0x100].all = TO_PHYS_U64(&pdpe) | 3;    // KERNEL_VMA: Present + Write (0x100 KERNEL_START_MEMORY)
    pml4e[0x1C0].all = TO_PHYS_U64(&pdpe_krnluser) | 3; // KERNEL_USER TO KRNLVMA: Present + Write (0x1C0 KERNEL_USER_VIEW_START_MEMORY)
    pdpe[0].all = TO_PHYS_U64(&pde) | 3; 
    pde[0].all = TO_PHYS_U64(&pte) | 3;
    pdpe_krnluser[0].all = TO_PHYS_U64(&pde_krnluser) | 3;
    pde_krnluser[0].all = TO_PHYS_U64(&pte_krnluser) | 3;


    uint64_t num_pages = TO_PHYS_U64(heap_end) / PAGE_SIZE;

    for (uint64_t i=0; i<=num_pages; i++) {
        pte[i].all = (i * PAGE_SIZE) | PAGE_PRESENT_CPL0; // Present + Write
    }
}

static void init_paging_user() {
    // User space
    memset(&pdpe_user, 0, sizeof(pdpe_t) * 512);
    memset(&pde_user, 0, sizeof(pde_t) * 512);
    memset(&pte_user, 0, sizeof(pte_t) * 512);

    pml4e[0].all = TO_PHYS_U64(&pdpe_user) | 0x7; // USER_VMA: Present + Write + CPL3
    pdpe_user[0].all = TO_PHYS_U64(&pde_user) | 0x07; 
    pde_user[0].all = TO_PHYS_U64(&pte_user) | 0x07;
}

static void init_paging_video() {
  uint64_t video_framebuffer = vesa_video_info.addr;
  uint64_t framebuffer_size = (uint32_t)vesa_video_info.pitch * vesa_video_info.height;
  uint64_t num_pages = (framebuffer_size + PAGE_SIZE - 1) / PAGE_SIZE;

  // Initialize the PDPE and PDE structures for the video memory mapping
  memset(&pdpe_video, 0, sizeof(pdpe_t) * 512);
  memset(&pde_video, 0, sizeof(pde_t) * 512);
  memset(&pte_video, 0, sizeof(pte_t) * 1024);

  // Set up the PML4E entry for the video memory region
  pml4e[0x180].all = TO_PHYS_U64(&pdpe_video) | PAGE_PRESENT_CPL0;

  // Link the PDPE to the PDE
  pdpe_video[0].all = TO_PHYS_U64(&pde_video) | PAGE_PRESENT_CPL0;

  // Two PDE entries: each covers 512 × 4KB = 2MB (framebuffer is 3MB at 1024×768×32bpp)
  pde_video[0].all = TO_PHYS_U64(&pte_video[0])   | PAGE_PRESENT_CPL0;
  pde_video[1].all = TO_PHYS_U64(&pte_video[512]) | PAGE_PRESENT_CPL0;

  for (uint64_t i = 0; i < num_pages; i++) {
      pte_video[i].all = (video_framebuffer + (i * PAGE_SIZE)) | PAGE_PRESENT_CPL0;
  }

  vesa_video_info.mapped = true;
}

void init_paging(){
    init_paging_kernel();
    init_paging_user();
    init_paging_video();
    x86_set_cr3(TO_PHYS_U64(pml4e));
    init_heap();
}
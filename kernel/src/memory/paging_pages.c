#include "memory/paging_pages.h"

pml4e_t pml4e[512] __attribute__((aligned(4096)));
pdpe_t  pdpe[512] __attribute__((aligned(4096)));
pde_t   pde[512] __attribute__((aligned(4096)));
pte_t   pte[2048] __attribute__((aligned(4096))); /* 4 PDEs × 512 = 8 MB */

pdpe_t  pdpe_user[512] __attribute__((aligned(4096)));
pde_t   pde_user[512] __attribute__((aligned(4096)));
pte_t   pte_user[512] __attribute__((aligned(4096)));

pdpe_t  pdpe_krnluser[512] __attribute__((aligned(4096)));
pde_t   pde_krnluser[512] __attribute__((aligned(4096)));
pte_t   pte_krnluser[512] __attribute__((aligned(4096)));

pdpe_t  pdpe_video[512] __attribute__((aligned(4096)));
pde_t   pde_video[512] __attribute__((aligned(4096)));
pte_t   pte_video[2048] __attribute__((aligned(4096)));

pdpe_t  pdpe_mmio[512] __attribute__((aligned(4096)));
pde_t   pde_mmio[512] __attribute__((aligned(4096)));
pte_t   pte_mmio[MMIO_POOL_PAGES] __attribute__((aligned(4096)));

pdpe_t  pdpe_apic[512] __attribute__((aligned(4096)));
pde_t   pde_apic[512] __attribute__((aligned(4096)));
pte_t   pte_apic[512] __attribute__((aligned(4096)));


static inline uint64_t x86_get_cr3(void) {
    uint64_t rv;

    __asm__ __volatile__ (
        "mov %%cr3, %0"
        : "=r" (rv));
    return rv;
}

uint64_t get_cr3(){
    return x86_get_cr3();
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

void tlb_flush(){
    x86_tlb_flush_all();
}

static void init_paging_kernel() {

    memset(&pml4e, 0, sizeof(pml4e_t) * 512);
    memset(&pdpe, 0, sizeof(pdpe_t) * 512);
    memset(&pde, 0, sizeof(pde_t) * 512);
    memset(&pte, 0, sizeof(pte_t) * 2048);
    memset(&pdpe_krnluser, 0, sizeof(pdpe_t) * 512);
    memset(&pde_krnluser, 0, sizeof(pde_t) * 512);
    memset(&pte_krnluser, 0, sizeof(pte_t) * 512);

    pml4e[0].all = TO_PHYS_U64(&pdpe) | 3;
    pml4e[0x100].all = TO_PHYS_U64(&pdpe) | 3;    // KERNEL_VMA: Present + Write (0x100 KERNEL_START_MEMORY)
    pml4e[0x1C0].all = TO_PHYS_U64(&pdpe_krnluser) | 3; // KERNEL_USER TO KRNLVMA: Present + Write (0x1C0 KERNEL_USER_VIEW_START_MEMORY)
    pdpe[0].all = TO_PHYS_U64(&pde) | 3;
    pde[0].all = TO_PHYS_U64(&pte[0])    | 3;  /* 0–2 MB   */
    pde[1].all = TO_PHYS_U64(&pte[512])  | 3;  /* 2–4 MB   */
    pde[2].all = TO_PHYS_U64(&pte[1024]) | 3;  /* 4–6 MB   */
    pde[3].all = TO_PHYS_U64(&pte[1536]) | 3;  /* 6–8 MB   */
    pdpe_krnluser[0].all = TO_PHYS_U64(&pde_krnluser) | 3;
    pde_krnluser[0].all = TO_PHYS_U64(&pte_krnluser) | 3;


    uint64_t num_pages = TO_PHYS_U64(heap_end) / PAGE_SIZE;
    if (num_pages > 2048) num_pages = 2048;

    for (uint64_t i = 0; i <= num_pages; i++) {
        pte[i].all = (i * PAGE_SIZE) | PAGE_PRESENT_CPL0;
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
  uint64_t video_framebuffer = video_info.addr;
  uint64_t framebuffer_size = (uint32_t)video_info.pitch * video_info.height;
  uint64_t num_pages = (framebuffer_size + PAGE_SIZE - 1) / PAGE_SIZE;

  // Initialize the PDPE and PDE structures for the video memory mapping
  memset(&pdpe_video, 0, sizeof(pdpe_t) * 512);
  memset(&pde_video, 0, sizeof(pde_t) * 512);
  memset(&pte_video, 0, sizeof(pte_t) * 2048);

  // Set up the PML4E entry for the video memory region
  pml4e[0x180].all = TO_PHYS_U64(&pdpe_video) | PAGE_PRESENT_CPL0;

  // Link the PDPE to the PDE
  pdpe_video[0].all = TO_PHYS_U64(&pde_video) | PAGE_PRESENT_CPL0;

  // Two PDE entries: each covers 512 × 4KB = 2MB (framebuffer is 8.3MB at 1920×1080×32bpp)
  pde_video[0].all = TO_PHYS_U64(&pte_video[0])   | PAGE_PRESENT_CPL0;
  pde_video[1].all = TO_PHYS_U64(&pte_video[512]) | PAGE_PRESENT_CPL0;
  pde_video[2].all = TO_PHYS_U64(&pte_video[1024]) | PAGE_PRESENT_CPL0;
  pde_video[3].all = TO_PHYS_U64(&pte_video[1536]) | PAGE_PRESENT_CPL0;

  for (uint64_t i = 0; i < num_pages; i++) {
      pte_video[i].all = (video_framebuffer + (i * PAGE_SIZE)) | PAGE_PRESENT_CPL0;
  }

  video_info.mapped = true;
}

extern uint8_t ap_trampoline_start[];
extern uint8_t ap_trampoline_end[];

void init_paging(){
    init_paging_kernel();
    init_paging_user();
    init_paging_video();
    x86_set_cr3(TO_PHYS_U64(pml4e));
    memcpy((void*)KERNEL_VMA + 0x8000, ap_trampoline_start, ap_trampoline_end - ap_trampoline_start);
}

static uint8_t mmio_bitmap[MMIO_POOL_PAGES / 8];  /* one bit per page */

void init_paging_mmio_pool(void) {
    memset(pdpe_mmio,   0, sizeof(pdpe_t) * 512);
    memset(pde_mmio,    0, sizeof(pde_t)  * 512);
    memset(pte_mmio,    0, sizeof(pte_t)  * MMIO_POOL_PAGES);
    memset(mmio_bitmap, 0, sizeof(mmio_bitmap));

    pml4e[0x1C8].all = TO_PHYS_U64(pdpe_mmio) | 3;
    pdpe_mmio[0].all = TO_PHYS_U64(pde_mmio)  | 3;
    pde_mmio[0].all  = TO_PHYS_U64(pte_mmio)  | 3;

    x86_tlb_flush_all();
}

/* Find n contiguous free pages; mark them used and return first index, or -1. */
static int mmio_alloc(uint32_t n) {
    for (uint32_t start = 0; start + n <= (uint32_t)MMIO_POOL_PAGES; ) {
        uint32_t i;
        for (i = 0; i < n; i++) {
            if (mmio_bitmap[(start + i) / 8] & (1u << ((start + i) & 7u))) {
                start += i + 1;
                break;
            }
        }
        if (i == n) {
            for (uint32_t j = 0; j < n; j++)
                mmio_bitmap[(start + j) / 8] |= 1u << ((start + j) & 7u);
            return (int)start;
        }
    }
    return -1;
}

void *paging_map_mmio(uint64_t phys, uint64_t size) {
    uint64_t offset       = phys & 0xFFF;
    uint64_t phys_aligned = phys & ~(uint64_t)0xFFF;
    uint64_t total        = (size + offset + 0xFFF) & ~(uint64_t)0xFFF;
    uint32_t npages       = (uint32_t)(total >> 12);

    int slot = mmio_alloc(npages);
    if (slot < 0) return NULL;

    for (uint32_t i = 0; i < npages; i++)
        pte_mmio[slot + i].all = (phys_aligned + (uint64_t)i * PAGE_SIZE) | 0x13;

    x86_tlb_flush_all();
    return (void *)(MMIO_POOL_BASE + (uint64_t)slot * PAGE_SIZE + offset);
}

void paging_unmap_mmio(void *virt, uint64_t size) {
    uint64_t v       = (uint64_t)virt;
    uint64_t voffset = v & 0xFFF;
    uint32_t slot    = (uint32_t)((v - voffset - MMIO_POOL_BASE) >> 12);
    uint64_t total   = (size + voffset + 0xFFF) & ~(uint64_t)0xFFF;
    uint32_t npages  = (uint32_t)(total >> 12);

    for (uint32_t i = 0; i < npages; i++) {
        pte_mmio[slot + i].all = 0;
        mmio_bitmap[(slot + i) / 8] &= ~(1u << ((slot + i) & 7u));
    }
    x86_tlb_flush_all();
}

void init_paging_apic(uint64_t lapic_phys,
                      uint64_t *ioapic_phys, uint32_t ioapic_count) {
    memset(pdpe_apic, 0, sizeof(pdpe_t)  * 512);
    memset(pde_apic,  0, sizeof(pde_t)   * 512);
    memset(pte_apic,  0, sizeof(pte_t)   * 512);

    pml4e[0x1E0].all = TO_PHYS_U64(pdpe_apic) | 3;
    pdpe_apic[0].all = TO_PHYS_U64(pde_apic)  | 3;
    pde_apic[0].all  = TO_PHYS_U64(pte_apic)  | 3;

    // 0x13 = present(0) | writable(1) | cache-disable(4)
    // Cache-disable is mandatory for MMIO: prevents speculative reads.
    pte_apic[0].all = (lapic_phys & ~(uint64_t)0xFFF) | 0x13;

    uint32_t count = ioapic_count < 4 ? ioapic_count : 4;
    for (uint32_t i = 0; i < count; i++)
        pte_apic[1 + i].all = (ioapic_phys[i] & ~(uint64_t)0xFFF) | 0x13;

    x86_tlb_flush_all();
}
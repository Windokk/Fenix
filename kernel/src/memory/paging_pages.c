#include "memory/paging_pages.h"
/*
TABLE -->         pml4 (in CR3) (= a table containing pmle4es (Page Map Level 4 Entries))
                    |                   |                                    |
ENTRIES -->      pml4es[0]        pml4es[256]                          pml4es[448]
                    |                   |                                    |
                    —————————————————————                                    |
                              |                                              |
TABLE -->      kernel_pdp (kernel_pdpes)(max 512 Go)              krnluser_pdp (krnl_pdpes)
                    |                                                       ...
ENTRY -->      kernel_pdpes[0]
                    |                    
TABLE -->      kernel_pd (kernel_pdes)  (max 1 Go)
                    |
ENTRY -->      kernel_pdes[0]       --> kernel_pdes[69]
                    |                   
TABLE -->      kernel_pt (kernel_ptes) (max 2 Mo)
                    |
      page0 | page1 | page2 | page 3...
*/

pml4e_t pml4es[512] __attribute__((aligned(4096)));

pdpe_t  kernel_pdpes[512] __attribute__((aligned(4096)));
pde_t   kernel_pdes[512] __attribute__((aligned(4096)));
pte_t   kernel_ptes[KERNEL_NUM_PDE * 512] __attribute__((aligned(4096))); /* 70 PDEs × 512 = 140 MB */

pdpe_t  user_pdpes[512] __attribute__((aligned(4096)));
pde_t   user_pdes[512] __attribute__((aligned(4096)));
pte_t   user_ptes[512] __attribute__((aligned(4096)));

pdpe_t  krnluser_pdpes[512] __attribute__((aligned(4096)));
pde_t   krnluser_pdes[512] __attribute__((aligned(4096)));
pte_t   krnluser_ptes[512] __attribute__((aligned(4096)));

pdpe_t  video_pdpes[512] __attribute__((aligned(4096)));
pde_t   video_pdes[512] __attribute__((aligned(4096)));
pte_t   video_ptes[2048] __attribute__((aligned(4096)));

pdpe_t  mmio_pdpes[512] __attribute__((aligned(4096)));
pde_t   mmio_pdes[512] __attribute__((aligned(4096)));
pte_t   mmio_ptes[MMIO_POOL_PAGES] __attribute__((aligned(4096)));

pdpe_t  apic_pdpes[512] __attribute__((aligned(4096)));
pde_t   apic_pdes[512] __attribute__((aligned(4096)));
pte_t   apic_ptes[512] __attribute__((aligned(4096)));


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

    memset(&pml4es, 0, sizeof(pml4es));
    memset(&kernel_pdpes, 0, sizeof(kernel_pdpes));
    memset(&kernel_pdes, 0, sizeof(kernel_pdes));
    memset(&kernel_ptes, 0, sizeof(kernel_ptes));
    memset(&krnluser_pdpes, 0, sizeof(krnluser_pdpes));
    memset(&krnluser_pdes, 0, sizeof(krnluser_pdes));
    memset(&krnluser_ptes, 0, sizeof(krnluser_ptes));

    pml4es[0].all = TO_PHYS_U64(&kernel_pdpes) | 3;
    pml4es[0x100].all = TO_PHYS_U64(&kernel_pdpes) | 3;    // KERNEL_VMA: Present + Write (0x100 KERNEL_START_MEMORY)
    kernel_pdpes[0].all = TO_PHYS_U64(&kernel_pdes) | 3;

    pml4es[0x1C0].all = TO_PHYS_U64(&krnluser_pdpes) | 3; // KERNEL_USER TO KRNLVMA: Present + Write (0x1C0 KERNEL_USER_VIEW_START_MEMORY)
    krnluser_pdpes[0].all = TO_PHYS_U64(&krnluser_pdes) | 3;
    krnluser_pdes[0].all = TO_PHYS_U64(&krnluser_ptes) | 3;

    for (uint64_t i = 0; i < KERNEL_NUM_PDE; i++) {

        kernel_pdes[i].all = TO_PHYS_U64(&kernel_ptes[i * 512]) | 3;

        for(uint64_t p = 0; p < 512; p++){
            kernel_ptes[(i * 512) + p].all = (((i * 512) + p) * PAGE_SIZE) | PAGE_PRESENT_CPL0;
        }
    }
}

static void init_paging_user() {
    // User space
    memset(&user_pdpes, 0, sizeof(user_pdpes));
    memset(&user_pdes, 0, sizeof(user_pdes));
    memset(&user_ptes, 0, sizeof(user_ptes));

    pml4es[0].all = TO_PHYS_U64(&user_pdpes) | 0x7; // USER_VMA: Present + Write + CPL3
    user_pdpes[0].all = TO_PHYS_U64(&user_pdes) | 0x07; 
    user_pdes[0].all = TO_PHYS_U64(&user_ptes) | 0x07;
}

static void init_paging_video() {
  uint64_t video_framebuffer = video_info.addr;
  uint64_t framebuffer_size = (uint32_t)video_info.pitch * video_info.height;
  uint64_t num_pages = (framebuffer_size + PAGE_SIZE - 1) / PAGE_SIZE;

  // Initialize the PDPE and PDE structures for the video memory mapping
  memset(&video_pdpes, 0, sizeof(video_pdpes));
  memset(&video_pdes, 0, sizeof(video_pdes));
  memset(&video_ptes, 0, sizeof(video_ptes));

  // Set up the pml4es entry for the video memory region
  pml4es[0x180].all = TO_PHYS_U64(&video_pdpes) | PAGE_PRESENT_CPL0;

  // Link the PDPE to the PDE
  video_pdpes[0].all = TO_PHYS_U64(&video_pdes) | PAGE_PRESENT_CPL0;

  // Two PDE entries: each covers 512 × 4KB = 2MB (framebuffer is 8.3MB at 1920×1080×32bpp)
  video_pdes[0].all = TO_PHYS_U64(&video_ptes[0])   | PAGE_PRESENT_CPL0;
  video_pdes[1].all = TO_PHYS_U64(&video_ptes[512]) | PAGE_PRESENT_CPL0;
  video_pdes[2].all = TO_PHYS_U64(&video_ptes[1024]) | PAGE_PRESENT_CPL0;
  video_pdes[3].all = TO_PHYS_U64(&video_ptes[1536]) | PAGE_PRESENT_CPL0;

  for (uint64_t i = 0; i < num_pages; i++) {
      video_ptes[i].all = (video_framebuffer + (i * PAGE_SIZE)) | PAGE_PRESENT_CPL0;
  }

  video_info.mapped = true;
}

extern uint8_t ap_trampoline_start[];
extern uint8_t ap_trampoline_end[];

void init_paging(){
    init_paging_kernel();
    init_paging_user();
    init_paging_video();
    x86_set_cr3(TO_PHYS_U64(pml4es));
    memcpy((void*)KERNEL_VMA + 0x8000, ap_trampoline_start, ap_trampoline_end - ap_trampoline_start);
}

static uint8_t mmio_bitmap[MMIO_POOL_PAGES / 8];  /* one bit per page */

void init_paging_mmio_pool(void)
{
    memset(mmio_pdpes,   0, sizeof(mmio_pdpes));
    memset(mmio_pdes,    0, sizeof(mmio_pdes));
    memset(mmio_ptes,    0, sizeof(mmio_ptes));
    memset(mmio_bitmap, 0, sizeof(mmio_bitmap));

    pml4es[0x1C8].all = TO_PHYS_U64(mmio_pdpes) | 3;
    mmio_pdpes[0].all = TO_PHYS_U64(mmio_pdes)  | 3;
    for(int i = 0; i < MMIO_NUM_PDPE; i++){
        mmio_pdes[i].all  = TO_PHYS_U64(&mmio_ptes[i * 512])  | 3;
    }

    x86_tlb_flush_all();
}

/* Find n contiguous free pages; mark them used and return first index, or -1. */
static int mmio_alloc(uint32_t n)
{
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

void *paging_map_mmio(uint64_t phys, uint64_t size)
{
    uint64_t offset       = phys & 0xFFF;
    uint64_t phys_aligned = phys & ~(uint64_t)0xFFF;
    uint64_t total        = (size + offset + 0xFFF) & ~(uint64_t)0xFFF;
    uint32_t npages       = (uint32_t)(total >> 12);

    int slot = mmio_alloc(npages);
    if (slot < 0) return NULL;

    for (uint32_t i = 0; i < npages; i++)
        mmio_ptes[slot + i].all = (phys_aligned + (uint64_t)i * PAGE_SIZE) | 0x13;

    x86_tlb_flush_all();
    return (void *)(MMIO_POOL_BASE + (uint64_t)slot * PAGE_SIZE + offset);
}

void paging_unmap_mmio(void *virt, uint64_t size)
{
    uint64_t v       = (uint64_t)virt;
    uint64_t voffset = v & 0xFFF;
    uint32_t slot    = (uint32_t)((v - voffset - MMIO_POOL_BASE) >> 12);
    uint64_t total   = (size + voffset + 0xFFF) & ~(uint64_t)0xFFF;
    uint32_t npages  = (uint32_t)(total >> 12);

    for (uint32_t i = 0; i < npages; i++) {
        mmio_ptes[slot + i].all = 0;
        mmio_bitmap[(slot + i) / 8] &= ~(1u << ((slot + i) & 7u));
    }
    x86_tlb_flush_all();
}

void init_paging_apic(uint64_t lapic_phys, uint64_t *ioapic_phys, uint32_t ioapic_count)
{
    memset(apic_pdpes, 0, sizeof(apic_pdpes));
    memset(apic_pdes,  0, sizeof(apic_pdes));
    memset(apic_ptes,  0, sizeof(apic_ptes));

    pml4es[0x1E0].all = TO_PHYS_U64(apic_pdpes) | 3;
    apic_pdpes[0].all = TO_PHYS_U64(apic_pdes)  | 3;
    apic_pdes[0].all  = TO_PHYS_U64(apic_ptes)  | 3;

    // 0x13 = present(0) | writable(1) | cache-disable(4)
    // Cache-disable is mandatory for MMIO: prevents speculative reads.
    apic_ptes[0].all = (lapic_phys & ~(uint64_t)0xFFF) | 0x13;

    uint32_t count = ioapic_count < 4 ? ioapic_count : 4;
    for (uint32_t i = 0; i < count; i++)
        apic_ptes[i + 1].all = (ioapic_phys[i] & ~(uint64_t)0xFFF) | 0x13;

    x86_tlb_flush_all();
}
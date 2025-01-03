#include "multiboot2.h"

#include "kernel.h"
#include "memory/paging_pages.h"

void HALT_AND_CATCH_FIRE(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    vprintf(fmt, args);
    va_end(args);
    x86_hlt(); 
    while(1) {}
}

void idle() {
  while(1) {
    x86_hlt();
  }
}

memory_infos_t parse_multiboot2_memory(multiboot_info_t *mbi) {
    multiboot_tag_t *tag;
    
    // Initialize memory_infos structure
    memory_infos_t mem_info;
    mem_info.memory_map_address = 0;
    mem_info.total_memory_size = 0;
    mem_info.usable_size = 0;
    mem_info.usable_start = 0;

    // Iterate through the multiboot tags
    for (
        tag = (multiboot_tag_t *) mbi->tags;
        tag->type != MULTIBOOT_TAG_TYPE_END;
        tag = (multiboot_tag_t *) ((uint8_t *) tag + ((tag->size + 7) & ~7))
    ) {
        // Check for memory map tag
        if (tag->type == MULTIBOOT_TAG_TYPE_MMAP) {
            struct multiboot_tag_mmap *mmap_tag = (struct multiboot_tag_mmap *)tag;

            // Memory map address is the base address of the memory map entries
            mem_info.memory_map_address = (uint64_t)mmap_tag->entries[0].addr;

            // Iterate through the memory map entries
            for (uint32_t i = 0; i < mmap_tag->size / mmap_tag->entry_size; i++) {
                struct multiboot_mmap_entry *entry = &mmap_tag->entries[i];

                // Add up the total memory size
                mem_info.total_memory_size += entry->len;

                // Check for usable memory regions (type = 1)
                if (entry->type == MULTIBOOT_MEMORY_AVAILABLE) {
                    if (mem_info.usable_start == 0) {
                        mem_info.usable_start = entry->addr;
                    }
                    mem_info.usable_size += entry->len;
                }
            }
            // Once we've processed the memory map, break the loop
            break;
        }
    }
    

    return mem_info;
}

// First parameter is passed via rdi, second via rsi
void kernel_premain(unsigned long magic, multiboot_info_t* mbi_phys) {
    // Store the pointers globally or use them directly
    

    init_serial();

    if(magic != MULTIBOOT2_BOOTLOADER_MAGIC){
        HALT_AND_CATCH_FIRE("--------- MAGIC NUMBER NOT RECOGNIZED ! ---------\nMAGIC NUMBER : 0x%X\n", (magic));
    }

    init_kernel_vesa(mbi_phys);
    
    memory_infos_t mem = parse_multiboot2_memory(mbi_phys);

    //Physical Memory
    init_frames(mem.usable_size);

    //Virtual Memory
    init_paging();

    uint32_t color = 0xFFFFFF; // White

    // Rectangle dimensions
    int startX = 20, startY = 20;
    int rectWidth = 40, rectHeight = 10;

    // Loop over the area of the rectangle and set each pixel
    for (int y = startY; y < startY + rectHeight; y++) {
        for (int x = startX; x < startX + rectWidth; x++) {
            ((uint32_t*)vesa_video_info.virt_addr)[y * vesa_video_info.width + x] = color;
        }
    }

    
    /*printf("Framebuffer: addr=%X, virt_addr=%X, width=%d, height=%d, bpp=%d\n", 
           vesa_video_info.addr,
           vesa_video_info.virt_addr,
           vesa_video_info.width,
           vesa_video_info.height, 
           vesa_video_info.bits);
    

    //Plot pixel at 20 20 (white)*/
    
}

void kernel_main()
{
    idle();
}
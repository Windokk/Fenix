#include "multiboot2.h"

#include "kernel.h"
#include "memory/paging_pages.h"

#include "interrupts/pic.h"
#include "interrupts/idt.h"

#include "drivers/keyboard.h"
#include "drivers/mouse.h"
#include "drivers/ps2.h"
#include "drivers/rtc.h"
#include "drivers/serial.h"
#include "drivers/vesa.h"

#include "graphics/gfx.h"

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

    pic_disable_all_irq();

    init_serial();

    printf("KERNEL_PREMAIN\n");

    if(magic != MULTIBOOT2_BOOTLOADER_MAGIC){
        HALT_AND_CATCH_FIRE("--------- MAGIC NUMBER NOT RECOGNIZED ! ---------\nMAGIC NUMBER RECIEVED : 0x%X\n", (magic));
    }

    printf("0x%X", mbi_phys);

    init_kernel_vesa(TO_VMA_PTR(multiboot_info_t *, mbi_phys));

    printf("---------- Memory Initialization ----------\n");

    memory_infos_t mem = parse_multiboot2_memory(mbi_phys);

    init_frames(mem.usable_size);
    init_paging();
    printf("Paging done\n");


    printf("\n");

    printf("------------ PIC Initialization ------------\n");


    pic_init(false);

    printf("\n");

    printf("------------ IDT Initialization ------------\n");

    // Setup Interrupt Descriptor table
    idt_init();

    printf("\n");

    disable_interrupts();

    // Unmask the 8259 Interrupt (necessary for mouse interrupts)
    pic_unmask_irq(PIC_8259_INTERRUPT);

    enable_interrupts();

    printf("\n");

    printf("------------ PS2 Initialization ------------\n\n");

    ps2_init();

    printf("\n\n");
    

    printf("--------- Keyboard Initialization ----------\n\n");

    keyboard_enable();

    printf("\n\n");

    printf("---------- Mouse Initialization ------------\n\n");

    mouse_enable();

    printf("\n\n");

    printf("----------- RTC Initialization -------------\n\n");

    rtc_init();

    printf("\n\n");

    enable_interrupts();
}

void kernel_main()
{
    rtc_time_t time = {
      0,
      0,
      0,
      0,
      0,
      0  
    };

    rtc_read_time(&time);
    
    printf("Current time : %u/%u/%u %u:%u:%u", time.month, time.day, time.year, time.hours, time.minutes, time.seconds);

    idle();
}
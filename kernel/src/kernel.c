#include "multiboot2.h"

#include "kernel.h"

#include "memory/paging_pages.h"

#include "tasks/task.h"
#include "tasks/shell.h"

#include "interrupts/pic.h"
#include "interrupts/idt.h"
#include "interrupts/apic.h"

#include "acpi/acpi.h"

#include "drivers/keyboard.h"
#include "drivers/mouse.h"
#include "drivers/ps2.h"
#include "drivers/rtc.h"
#include "drivers/serial.h"
#include "drivers/vesa.h"
#include "drivers/hpet.h"

#include "graphics/gfx.h"

// Saved physical pointer to the multiboot2 info structure.
// init_paging() maps the first 8 MB at VMA, so acpi_init() can access it
// via TO_VMA_U64 as long as GRUB placed it below 8 MB (always true in practice).
static multiboot_info_t *g_mbi_phys = NULL;

void HALT_AND_CATCH_FIRE(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    vprintf(fmt, args);
    va_end(args);
    x86_hlt(); 
    while(1) {}
}

static void idle_task(void) {
    while (1) {
        x86_hlt();
        task_yield();
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

    KLOG_INFO("KERNEL", "Starting premain...");

    if(magic != MULTIBOOT2_BOOTLOADER_MAGIC){
        KLOG_FATAL("KERNEL","MAGIC NUMBER NOT RECOGNIZED !\nMAGIC NUMBER RECIEVED : 0x%X\n", (magic));
    }

    g_mbi_phys = mbi_phys;

    init_kernel_vesa(TO_VMA_PTR(multiboot_info_t *, mbi_phys));

    KLOG_INFO("KERNEL", "Memory Initialization");

    memory_infos_t mem = parse_multiboot2_memory(mbi_phys);

    init_frames(mem.usable_size);
    init_paging();
    
    KLOG_INFO("KERNEL", "Done.");

    KLOG_INFO("KERNEL", "ACPI Initialization");

    acpi_init(g_mbi_phys);

    KLOG_INFO("KERNEL", "Done.");
    
    KLOG_INFO("KERNEL", "HPET Initialization");

    hpet_init();

    KLOG_INFO("KERNEL", "Done.");
    
    KLOG_INFO("KERNEL", "PIC Initialization");

    pic_init(false);

    KLOG_INFO("KERNEL", "Done.");
    
    KLOG_INFO("KERNEL", "IDT Initialization");

    // Setup Interrupt Descriptor table
    idt_init();

    KLOG_INFO("KERNEL", "Done.");
    
    KLOG_INFO("KERNEL", "APIC Initialization");

    // Try to switch from legacy 8259 PIC to LAPIC + I/O APIC.
    // If ACPI didn't find a MADT, apic_init() is a no-op and we stay on PIC.
    apic_init();

    if (!g_use_apic) {
        disable_interrupts();
        // Legacy PIC path: unmask cascade and device IRQs
        pic_unmask_irq(PIC_8259_INTERRUPT);
        enable_interrupts();
    }

    KLOG_INFO("KERNEL", "Done.");

    KLOG_INFO("KERNEL", "PS/2 Initialization");

    ps2_init();

    KLOG_INFO("KERNEL", "Done.");

    KLOG_INFO("KERNEL", "KEYBOARD Initialization");

    keyboard_enable();

    KLOG_INFO("KERNEL", "Done.");

    KLOG_INFO("KERNEL", "Mouse Initialization");

    mouse_enable();

    KLOG_INFO("KERNEL", "Done.");

    KLOG_INFO("KERNEL", "RTC Initialization");

    rtc_init();

    KLOG_INFO("KERNEL", "Done.");

    enable_interrupts();
}

void kernel_main()
{
    task_init();
    task_create(shell_task, "shell");
    task_create(idle_task,  "idle");

    KLOG_INFO("KERNEL", "Task system ready");

    scheduler_start();
    /* never reached */
}
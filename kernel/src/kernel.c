#include "multiboot2.h"

#include "kernel.h"
#include "memory/paging_pages.h"
#include "tasks/task.h"
#include "tasks/shell.h"

#include "interrupts/pic.h"
#include "interrupts/idt.h"
#include "interrupts/apic.h"

#include "acpi/acpi.h"

#include <uacpi/uacpi.h>
#include <uacpi/event.h>
#include <uacpi/sleep.h>
#include "acpi/uacpi_glue.h"

#include "drivers/keyboard.h"
#include "drivers/mouse.h"
#include "drivers/ps2.h"
#include "drivers/rtc.h"
#include "drivers/serial.h"
#include "drivers/vesa.h"

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

static uacpi_interrupt_ret power_button_handler(uacpi_handle ctx) {
    (void)ctx;
    printf("[ACPI] Power button pressed — shutting down\n");
    acpi_enter_sleep(5);
    return UACPI_INTERRUPT_HANDLED;
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

    printf("KERNEL_PREMAIN\n");

    if(magic != MULTIBOOT2_BOOTLOADER_MAGIC){
        HALT_AND_CATCH_FIRE("--------- MAGIC NUMBER NOT RECOGNIZED ! ---------\nMAGIC NUMBER RECIEVED : 0x%X\n", (magic));
    }

    g_mbi_phys = mbi_phys;

    init_kernel_vesa(TO_VMA_PTR(multiboot_info_t *, mbi_phys));

    printf("---------- Memory Initialization ----------\n");

    memory_infos_t mem = parse_multiboot2_memory(mbi_phys);

    init_frames(mem.usable_size);
    init_paging();
    printf("Paging done\n");

    printf("------------ ACPI Initialization -----------\n");

    acpi_init(g_mbi_phys);

    printf("------------ PIC Initialization ------------\n");

    pic_init(false);

    printf("------------ IDT Initialization ------------\n");

    // Setup Interrupt Descriptor table
    idt_init();

    printf("------------ APIC Initialization -----------\n");

    // Try to switch from legacy 8259 PIC to LAPIC + I/O APIC.
    // If ACPI didn't find a MADT, apic_init() is a no-op and we stay on PIC.
    apic_init();

    if (!g_use_apic) {
        disable_interrupts();
        // Legacy PIC path: unmask cascade and device IRQs
        pic_unmask_irq(PIC_8259_INTERRUPT);
        enable_interrupts();
    }


    printf("------------ PS2 Initialization ------------\n");

    ps2_init();

    printf("--------- Keyboard Initialization ----------\n");

    keyboard_enable();

    printf("---------- Mouse Initialization ------------\n");

    mouse_enable();

    printf("----------- RTC Initialization -------------\n");

    rtc_init();

    enable_interrupts();

    printf("---------- uACPI Initialization ----------\n");
    serial_print("[uACPI] calling uacpi_initialize\n");

    /* UACPI_FLAG_NO_ACPI_MODE: acpi_init() already enabled ACPI mode via SMI_CMD */
    uacpi_status ust = uacpi_initialize(UACPI_FLAG_NO_ACPI_MODE);
    serial_print("[uACPI] initialize done\n");
    if (ust == UACPI_STATUS_OK) {
        serial_print("[uACPI] calling namespace_load\n");
        ust = uacpi_namespace_load();
        serial_print("[uACPI] namespace_load done\n");
        if (ust == UACPI_STATUS_OK) {
            serial_print("[uACPI] calling namespace_initialize\n");
            ust = uacpi_namespace_initialize();
            serial_print("[uACPI] namespace_initialize done\n");
            if (ust == UACPI_STATUS_OK) {
                g_uacpi_ready = true;
                printf("[uACPI] Full namespace initialized\n");
                uacpi_install_fixed_event_handler(
                    UACPI_FIXED_EVENT_POWER_BUTTON,
                    power_button_handler, UACPI_NULL);
            } else {
                printf("[uACPI] namespace_initialize failed: 0x%X\n", (uint32_t)ust);
            }
        } else {
            printf("[uACPI] namespace_load failed: 0x%X\n", (uint32_t)ust);
        }
    } else {
        printf("[uACPI] initialize failed: 0x%X\n", (uint32_t)ust);
    }
}

void kernel_main()
{
    rtc_time_t time = {0,0,0,0,0,0};
    rtc_read_time(&time);
    printf("Current time : %u/%u/%u %u:%u:%u\n",
           time.month, time.day, time.year,
           time.hours, time.minutes, time.seconds);

    task_init();
    task_create(shell_task, "shell");
    task_create(idle_task,  "idle");

    printf("---------- Task System Ready ----------\n");

    scheduler_start();
    /* never reached */
}
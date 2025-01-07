#include "drivers/keyboard.h"

#include "interrupts/pic.h"
#include "interrupts/idt.h"

#include "drivers/ps2.h"

#include "stdio.h"

void keyboard_enable() {
    
    disable_interrupts();
    pic_unmask_irq(PIC_KEYBOARD_INTERRUPT);
    enable_interrupts();
    printf("Keyboard IRQ enabled\n");
}

void keyboard_handle_irq() {

    /* bit 7 is high when key is released */
    char* event;
    uint8_t scan_code = inb(0x60);
    if (!((scan_code >> 7) & 1ULL)) {
        event = "pressed";
    } else {
        event = "released";
    }
    printf("Keyboard %s %u\n", event, scan_code);

    /* Acknowledge that we've received the interrupt */
    pic_send_eoi(PIC_KEYBOARD_INTERRUPT);
}
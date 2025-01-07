#include "interrupts/pic.h"

#include "stdio.h"

static bool pic_debugging = true;

void pic_init(bool debugging) {
    /* Send ICW1 to both PIC chips */
    outb(PIC1_COMMAND, ICW1_INIT | ICW1_CALLADDR_4 | ICW1_ICW4_NEEDED);
    outb(PIC2_COMMAND, ICW1_INIT | ICW1_CALLADDR_4 | ICW1_ICW4_NEEDED);

    /* Determine the offset */
    outb(PIC1_DATA, 32);
    outb(PIC2_DATA, 40);

    /* Configure active/passive device cascade */
    outb(PIC1_DATA, 4);
    outb(PIC2_DATA, 2);

    /* Set operation mode - 8086 mode on*/
    outb(PIC1_DATA, 1);
    outb(PIC2_DATA, 1);

    /* mask (disable) all PIC interrupts */
    pic_disable_all_irq();

    if(pic_debugging){
        printf("PIC initialised\n");
    }

    pic_debugging = debugging;
}

void pic_send_eoi(uint8_t isa_irq) {
    /* Passive PIC */
    if (isa_irq >= 8)
        outb(PIC2_COMMAND, PIC_EOI);

    /* Active PIC */
    outb(PIC1_COMMAND, PIC_EOI);
}

void pic_unmask_irq(uint8_t isa_irq) {
    uint64_t pic_selector;
    if (isa_irq < 8)
        pic_selector = PIC1_DATA;
    else {
        pic_selector = PIC2_DATA;
        isa_irq -= 8;
    }

    uint8_t value = inb(pic_selector);
    
    if(pic_debugging){
        printf("PIC Addr: 0x%x mask state: %u\n", pic_selector, value);
    }
    if (((value >> isa_irq) & 1ULL)) {
        value = value & ~(1UL << isa_irq); // Corrected assignment
        if(pic_debugging){
            printf("PIC Addr: 0x%x new mask State: %u\n", pic_selector, value);
        }
        outb(pic_selector, value);
        if(pic_debugging){
            printf("IRQ %u unmasked\n", isa_irq);
        }
    }
}


void pic_mask_irq(uint8_t isa_irq) {
    uint64_t pic_selector;
    if (isa_irq < 8)
        pic_selector = PIC1_DATA;
    else {
        pic_selector = PIC2_DATA;
        isa_irq -= 8;
    }

    uint8_t value = inb(pic_selector);
    
    if(pic_debugging){
        printf("PIC Addr: 0x%X mask state: %u\n", pic_selector, value);
    }

    // Check if the bit for the specified IRQ is not already set
    if (!((value >> isa_irq) & 1ULL)) {
        value |= (1ULL << isa_irq); // Set the bit to mask the IRQ
        if(pic_debugging){
            printf("PIC Addr: 0x%X new mask State: %u\n", pic_selector, value);
        }
        outb(pic_selector, value);
        if(pic_debugging){
            printf("IRQ %u masked\n", isa_irq); 
        }
    }
}

static uint16_t pic_get_irq_reg(uint8_t ocw3) {
    outb(PIC1_COMMAND, ocw3);
    outb(PIC2_COMMAND, ocw3);
    return (inb(PIC2_COMMAND) << 8) | inb(PIC1_COMMAND);
}

uint16_t pic_read_isr(void) {
    return pic_get_irq_reg(PIC_READ_ISR);
}

void pic_disable_all_irq(void) {
    outb(PIC1_DATA, 0xff);
    outb(PIC2_DATA, 0xff);
}

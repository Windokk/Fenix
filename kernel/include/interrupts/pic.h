#pragma once

#include "common.h"

#include "drivers/serial.h"

/*  General IBM-PC Compatible Interrupt Information - Standard ISA IRQs  */
#define PIC_PROG_INT_TIMER_INTERRUPT        0
#define PIC_KEYBOARD_INTERRUPT              1
#define PIC_8259_INTERRUPT                  2
#define PIC_COM2_INTERRUPT                  3
#define PIC_COM1_INTERRUPT                  4
#define PIC_LPT2_INTERRUPT                  5
#define PIC_FLOPPY_DISK_INTERRUPT           6
#define PIC_LPT1_OR_SPURIOUS_INTERRUPT      7
#define PIC_CMOS_REALTIME_CLOCK_INTERRUPT   8
#define PIC_PS2_MOUSE_INTERRUPT             12
#define PIC_FPU_COPROC_INTERPROC_INTERRUPT  13
#define PIC_PRIMARY_ATA_HD_INTERRUPT        14
#define PIC_SECONDARY_ATA_HD_INTERRUPT      15

#define PIC1            0x20        /* IO base address for Active PIC */
#define PIC2            0xA0        /* IO base address for Passive PIC */
#define PIC1_COMMAND    PIC1
#define PIC1_DATA       PIC1 + 1
#define PIC2_COMMAND    PIC2
#define PIC2_DATA       PIC2 + 1
#define PIC_EOI         0x20        /* End-of-interrupt command code */
#define PIC_READ_IRR    0x0a        /* OCW3 irq ready next CMD read */
#define PIC_READ_ISR    0x0b        /* OCW3 irq service next CMD read */

#define ICW1_INIT               (0 << 8) | (1 << 4)
#define ICW1_ICW4_NEEDED        1
#define ICW1_CALLADDR_4         1 << 2

void pic_init(bool debugging);
void pic_send_eoi(uint8_t isa_irq);
void pic_unmask_irq(uint8_t isa_irq);
void pic_mask_irq(uint8_t isa_irq);
uint16_t pic_read_isr(void);
void pic_disable_all_irq(void);
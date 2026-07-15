#include "interrupts/idt.h"

#include "stdio.h"

#include "interrupts/pic.h"
#include "interrupts/apic.h"   /* also pulls in acpi/acpi.h → g_acpi, g_use_apic */
#include "interrupts/lapic_timer.h"

#include "drivers/ps2/ps2_keyboard.h"  /* also pulls in tasks/task.h → current_task, task_* */
#include "drivers/ps2/ps2_mouse.h"
#include "drivers/rtc.h"

#include "acpi/acpi.h"
#include "acpi/uacpi_glue.h"

#include "tasks/task.h"

#include "profiling/profiling.h"

#include "cpu/smp.h"

extern void* msi_stubs[175];

static void (*irq_handlers[256])(isr_ctx_t*) = {0};

void enable_interrupts()
{
    asm volatile("sti");
}

uintptr_t disable_interrupts() {

    uintptr_t flags;
    asm volatile("pushfq; pop %0; cli" : "=r"(flags) :: "memory");
    return flags;
}

void restore_interrupts(uintptr_t interrupts_state)
{
    asm volatile("push %0; popfq" :: "r"(interrupts_state) : "memory");
}

static idt_gate_t idt[IDT_ENTRIES];
static idt_register_t idt_reg;

static const char *exception_strs[] = {
        //  Intel 64 Manual Volume 2 - Table 6-1 -> Exceptions and Interrupts
        "#DE: Integer Divide-by-Zero Exception",
        "#DB: Debug exception",
        "Non-maskable interrupt",
        "#BP: Breakpoint Exception (INT 3)",
        "#OF: Overflow Exception (INTO instruction)",
        "#BR: Bound-Range Exception (BOUND instruction)",
        "#UD: Invalid opcode exception",
        "#NM: Device-Not-Available Exception",
        "#DF: Double-Fault Exception",
        "Coprocessor segment overrun (reserved in AMD64)",
        "#TS: Invalid-TSS Exception",
        "#NP: Segment-Not-Present Exception",
        "#SS: Stack exception",
        "#GP: General-Protection exception",
        "#PF: Page-Fault exception",
        "(Reserved)",
        "#MF: x87 FPU Floating-Point error",
        "#AC: Alignment-Check exception",
        "#MC: Machine-Check exception",
        "#XM: SIMD Floating-Point exception",
        "#VE: Virtualisation Exception",
        "#CP: Control-Protection Exception",
};

static void config_idt_vector(uint16_t n, uint64_t handler) {
    idt[n].selector    = 0x08;
    idt[n].offset_low  = (uint16_t) handler;
    idt[n].offset_mid  = (uint16_t) (handler >> 16);
    idt[n].offset_high = (uint32_t) (handler >> 32);

    idt[n].ist.ist      = 0; // do not switch stack
    idt[n].ist.reserved = 0;

    idt[n].type_attr.present   = 1;    // valid
    idt[n].type_attr.dpl       = 0;    // kernel only
    idt[n].type_attr.gate_type = 0xE;  // 0xE = interrupt gate, 0xF = trap gate
    idt[n].type_attr.zero      = 0;

    idt[n].reserved = 0;
}

void idt_init() {

    config_idt_vector(0, (uintptr_t) vector0);
    config_idt_vector(1, (uintptr_t) vector1);
    config_idt_vector(2, (uintptr_t) vector2);
    config_idt_vector(3, (uintptr_t) vector3);
    config_idt_vector(4, (uintptr_t) vector4);
    config_idt_vector(5, (uintptr_t) vector5);
    config_idt_vector(6, (uintptr_t) vector6);
    config_idt_vector(7, (uintptr_t) vector7);
    config_idt_vector(8, (uintptr_t) vector8);
    config_idt_vector(10, (uintptr_t) vector10);
    config_idt_vector(11, (uintptr_t) vector11);
    config_idt_vector(12, (uintptr_t) vector12);
    config_idt_vector(13, (uintptr_t) vector13);
    config_idt_vector(14, (uintptr_t) vector14);
    config_idt_vector(16, (uintptr_t) vector16);
    config_idt_vector(17, (uintptr_t) vector17);
    config_idt_vector(18, (uintptr_t) vector18);
    config_idt_vector(19, (uintptr_t) vector19);
    config_idt_vector(20, (uintptr_t) vector20);
    config_idt_vector(21, (uintptr_t) vector21);
    // Legacy pit timer
    config_idt_vector(32, (uintptr_t) vector32);
    // Keyboard
    config_idt_vector(33, (uintptr_t) vector33);

    config_idt_vector(34, (uintptr_t) vector34);
    config_idt_vector(35, (uintptr_t) vector35);
    config_idt_vector(36, (uintptr_t) vector36);
    config_idt_vector(37, (uintptr_t) vector37);
    config_idt_vector(38, (uintptr_t) vector38);
    config_idt_vector(39, (uintptr_t) vector39);
    config_idt_vector(40, (uintptr_t) vector40);
    config_idt_vector(41, (uintptr_t) vector41);
    config_idt_vector(42, (uintptr_t) vector42);
    config_idt_vector(43, (uintptr_t) vector43);
    // Mouse
    config_idt_vector(44, (uintptr_t) vector44);

    //LAPIC Timer / Scheduler tick
    config_idt_vector(64, (uintptr_t) vector64);

    config_idt_vector(0xF0, (uintptr_t) vector240);

    for (int i = 0; i < 175; i++)
        config_idt_vector(0x41 + i, (uintptr_t)msi_stubs[i]);

    KLOG_INFO("IDT","Loading IDT");
    load_idt();
}

inline static void load_idt_internal() {
    idt_reg.base = (uint64_t) &idt;
    idt_reg.length = IDT_ENTRIES * sizeof(idt_gate_t) - 1;
    __asm__("lidt %0" : : "m"(idt_reg));
}

void load_idt()
{
    load_idt_internal();
}

void idt_register_irq_handler(uint8_t vector, void (*handler)(isr_ctx_t *))
{
    irq_handlers[vector] = handler;
}

void interrupt_handler(isr_ctx_t *regs)
{

#ifdef KERNEL_PROFILING
    uint64_t t0 = rdtsc();
    #endif

    uint8_t int_no   = regs->int_no;
    bool    preempt  = false;

    if (int_no == 33) {
        if (!g_use_apic) pic_mask_irq(PIC_KEYBOARD_INTERRUPT);
        ps2_keyboard_handle_irq();
        if (!g_use_apic) pic_unmask_irq(PIC_KEYBOARD_INTERRUPT);
    }
    else if (int_no == 40) {
        rtc_handle_irq();
    }
    else if (int_no == 44) {
        if (!g_use_apic) pic_mask_irq(PIC_PS2_MOUSE_INTERRUPT);
        ps2_mouse_handle_irq();
        if (!g_use_apic) pic_unmask_irq(PIC_PS2_MOUSE_INTERRUPT);
    }
    else if (g_acpi.initialized && int_no == (uint8_t)(32u + g_acpi.sci_irq)) {
        // SCI: dispatch through uACPI when ready, else hand-rolled handler
        uacpi_sci_dispatch();
    }
    else if(int_no == 64){
        lapic_timer_handle_irq();
        preempt = true;  // preempt after EOI, not before
    }
    else if(int_no == 0xF0){
        if (g_use_apic) {
            lapic_send_eoi();
            task_yield_local();
        }
        return;

    }
    else if (int_no >= 0x41 && int_no <= 0xEF) {
        if (irq_handlers[int_no])
            irq_handlers[int_no](regs);
        if (g_use_apic) lapic_send_eoi();
        return;
    }
    else if (int_no >= 32) {
        // Unknown hardware IRQ — just EOI and return
        if (g_use_apic) lapic_send_eoi();
        return;
    }
    else {
        disable_interrupts();
        KLOG_ERROR("IDT","Error: %s, Error Code: 0x%x", exception_strs[int_no], regs->error_code);
        for (;;) {
            asm volatile ("hlt");
        }
    }

    #ifdef KERNEL_PROFILING
    prof_record(PROF_ISR, rdtsc() - t0);
    #endif

    /*
    * Send EOI before potentially yielding.  If we context-switch here the
    * LAPIC in-service bit must already be cleared; otherwise the LAPIC will
    * not deliver any further interrupts until this task runs again.
    */
    if (g_use_apic) lapic_send_eoi();

    uint32_t cpu_id = cpu_self_id();

    if (preempt && cpu_local[cpu_id].current_task) {
        task_tick_update_local();  // wake any tasks whose sleep timer expired
        task_yield_local();        // preemptive round-robin
    }
}

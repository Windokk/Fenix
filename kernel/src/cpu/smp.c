#include "cpu/smp.h"

#include "acpi/acpi.h"

#include "memory/paging_pages.h"

#include "interrupts/apic.h"
#include "interrupts/idt.h"
#include "interrupts/lapic_timer.h"

#include "tasks/task.h"

volatile uint32_t g_ap_online_count = 1;
volatile uint32_t g_ap_ready_count = 1;

tss_t       cpu_tss[MAX_CPUS];
cpu_gdt_t   cpu_gdt[MAX_CPUS];

cpu_local_t cpu_local[MAX_CPUS];

void smp_start_ap(uint8_t apic_id, uint8_t trampoline_page) {
    lapic_send_init(apic_id);
    wait_ms(10);               // Intel SDM : 10ms
    lapic_send_sipi(apic_id, trampoline_page);
    wait_ms(1);
    if (g_ap_ready_count < g_acpi.cpu_count) {
        lapic_send_sipi(apic_id, trampoline_page);
        wait_ms(1);
    }
}

uint64_t create_descriptor(uint32_t base, uint32_t limit, uint16_t flag)
{
    uint64_t descriptor;
 
    // Create the high 32 bit segment
    descriptor  =  limit       & 0x000F0000;         // set limit bits 19:16
    descriptor |= (flag <<  8) & 0x00F0FF00;         // set type, p, dpl, s, g, d/b, l and avl fields
    descriptor |= (base >> 16) & 0x000000FF;         // set base bits 23:16
    descriptor |=  base        & 0xFF000000;         // set base bits 31:24
 
    // Shift by 32 to allow for low part of segment
    descriptor <<= 32;
 
    // Create the low 32 bit segment
    descriptor |= base  << 16;                       // set base bits 15:0
    descriptor |= limit  & 0x0000FFFF;               // set limit bits 15:0
 
    return descriptor;
}

tss_t create_tss(uint64_t stack_top) {
    tss_t tss      = {0};
    tss.rsp0       = stack_top;
    tss.iomap_base = sizeof(tss_t);
    return tss;
}

void create_gdt(int cpu_id, uint64_t stack_top){
    if(cpu_id >= 0 && cpu_id < MAX_CPUS){
        uint64_t null = create_descriptor(0, 0, 0);
        uint64_t kernel_code = create_descriptor(0, 0x000FFFFF, (GDT_CODE_PL0));
        uint64_t kernel_data = create_descriptor(0, 0x000FFFFF, (GDT_DATA_PL0));
        uint64_t user_code = create_descriptor(0, 0x000FFFFF, (GDT_CODE_PL3));
        uint64_t user_data = create_descriptor(0, 0x000FFFFF, (GDT_DATA_PL3));

        tss_t tss = create_tss(stack_top);

        uint64_t base  = (uint64_t)&cpu_tss[cpu_id];
        uint32_t limit = sizeof(tss_t) - 1;

        uint64_t tss_low =
            (uint64_t)(limit & 0xFFFF)
            | ((base & 0xFFFFFFULL) << 16)
            | (0x89ULL << 40)                          // present, DPL=0, available 64-bit TSS
            | ((uint64_t)((limit >> 16) & 0xF) << 48)
            | (((base >> 24) & 0xFFULL) << 56);

        uint64_t tss_high = (base >> 32) & 0xFFFFFFFFULL;

        cpu_tss[cpu_id] = tss;
        cpu_gdt_t gdt = {{null, kernel_code, kernel_data, user_code, user_data}, tss_low, tss_high};
        cpu_gdt[cpu_id] = gdt;
    }
}

static void lapic_init_ap(void) {
    // Enable the LAPIC via the Spurious Interrupt Vector Register
    lapic_write(LAPIC_SVR, LAPIC_SVR_ENABLE | LAPIC_SPURIOUS_VEC);

    // Accept all interrupt priority levels
    lapic_write(LAPIC_TPR, 0);

    // Mask timers and performance counters (we're not using LAPIC timer yet)
    lapic_write(LAPIC_LVT_TIMER,   LAPIC_LVT_MASKED);
    lapic_write(LAPIC_LVT_THERMAL, LAPIC_LVT_MASKED);
    lapic_write(LAPIC_LVT_PERF,    LAPIC_LVT_MASKED);

    // LINT0: external interrupt (masked — I/O APIC takes over)
    lapic_write(LAPIC_LVT_LINT0, LAPIC_LVT_MASKED);

    // LINT1: NMI delivery (standard PC wiring)
    lapic_write(LAPIC_LVT_LINT1, LAPIC_LVT_NMI);

    // Error vector
    lapic_write(LAPIC_LVT_ERROR, 0x27);

    // Clear any stale errors and pending EOI
    lapic_write(LAPIC_ESR, 0);
    lapic_write(LAPIC_EOI, 0);

    KLOG_INFO("APIC","LAPIC enabled  ID=0x%X  version=0x%X",
           (lapic_read(LAPIC_ID) >> 24) & 0xFF,
           lapic_read(LAPIC_VER) & 0xFF);
}

void ap_main(uint64_t ap_stack_top) {

    uint32_t current_id = __atomic_fetch_add(&g_ap_online_count, 1, __ATOMIC_SEQ_CST);

    lapic_init_ap();
    load_idt();

    create_gdt(current_id, ap_stack_top);

    gdtr_t gdtr;
    gdtr.limit = sizeof(cpu_gdt_t) - 1;
    gdtr.base  = (uint64_t)&cpu_gdt[current_id];
    __asm__ volatile("lgdt %0" : : "m"(gdtr));

    __asm__ volatile(
        "pushq $0x08\n"
        "leaq 1f(%%rip), %%rax\n"
        "pushq %%rax\n"
        "lretq\n"
        "1:\n"
        "mov $0x10, %%ax\n"
        "mov %%ax, %%ds\n"
        "mov %%ax, %%es\n"
        "mov %%ax, %%ss\n"
        : : : "rax", "memory"
    );

    __asm__ volatile("ltr %0" : : "r"((uint16_t)0x28));

    cpu_local[cpu_self_id()].cpu_id = cpu_self_id();

    if(current_id == g_acpi.cpu_count - 1){
        user_ptes[8].all = 0;
        tlb_flush();
    }

    task_init_local();

    task_create_local(idle_task, "idle");

    KLOG_INFO("SMP", "Task system ready for AP %u", cpu_self_id());

    KLOG_INFO("SMP", "AP online  APIC_ID=0x%X", (int)(lapic_read(LAPIC_ID) >> 24) & 0xFF);
    
    __atomic_fetch_add(&g_ap_ready_count, 1, __ATOMIC_SEQ_CST);

    scheduler_start_local();

    // Never reached
    for (;;) __asm__ volatile("hlt");
}

void smp_start_all(void){

    user_ptes[8].all = 0x8000 | PAGE_PRESENT_CPL0;
    tlb_flush();

    uint32_t bsp_id = ((lapic_read(LAPIC_ID) >> 24) & 0xFF);

    if(g_acpi.cpu_count > 0){
        for(uint32_t i = 0; i < g_acpi.cpu_count; i++){

            if (g_acpi.cpus[i].apic_id == bsp_id) continue;
                
            void* ptr = kmalloc_aligned(AP_STACK_SIZE, 16);
            uint64_t ap_stack_top = (uint64_t)ptr + AP_STACK_SIZE;

            void* base = (void*)((uint64_t)KERNEL_VMA + 0x8000);
            *(uint64_t*)(base + 0x4E) = (uint64_t)get_cr3();
            *(uint64_t*)(base + 0x56) = ap_stack_top;
            *(uint64_t*)(base + 0x5E) = (uint64_t)ap_main;

            KLOG_INFO("SMP", "Waking up AP %u", i);

            smp_start_ap(g_acpi.cpus[i].apic_id, 0x08);
        }
    }
}

uint32_t cpu_self_id()
{
    if (!g_lapic_ready) return 0;

    uint32_t lapic_id = ((lapic_read(LAPIC_ID) >> 24) & 0xFF);
    for(uint32_t i = 0; i < ACPI_MAX_CPUS; i++){
        if(g_acpi.cpus[i].apic_id == lapic_id){
            return i;
        }
    }
    KLOG_FATAL("SMP","cpu_self_id: APIC ID not found");
    return 0;
}

uint32_t cpu_least_loaded(){
    uint32_t min_run_count = UINT32_MAX;
    uint32_t ret_cpu = 0;
    for(uint32_t i = 0; i < g_acpi.cpu_count; i++){
        if(cpu_local[i].run_count < min_run_count && cpu_local[i].stack_pool != NULL){
            ret_cpu = cpu_local[i].cpu_id;
            min_run_count = cpu_local[i].run_count;
        }
    }
    return ret_cpu;
}
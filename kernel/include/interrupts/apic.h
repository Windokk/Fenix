#pragma once

#include "common.h"
#include "drivers/serial.h"
#include "acpi/acpi.h"

// ─── Virtual addresses for APIC MMIO ─────────────────────────────────────────
//
// P4[0x1E0] → 0xFFFFF00000000000
//   page 0: LAPIC       (0xFFFFF00000000000)
//   page 1: IOAPIC #0   (0xFFFFF00000001000)
//   page 2: IOAPIC #1   (0xFFFFF00000002000)
//   ...up to ACPI_MAX_IOAPICS IOAPICs

#define APIC_MMIO_BASE     0xFFFFF00000000000ULL
#define LAPIC_VIRT_BASE    (APIC_MMIO_BASE + 0x0000ULL)  // BSP CPU LAPIC ADDRESS
#define IOAPIC_VIRT_BASE   (APIC_MMIO_BASE + 0x1000ULL)  // IOAPIC #0
#define IOAPIC_VIRT_STRIDE 0x1000ULL                     // 4 KB per IOAPIC

// ─── LAPIC register offsets (relative to LAPIC_VIRT_BASE) ────────────────────

#define LAPIC_ID          0x020   // LAPIC ID
#define LAPIC_VER         0x030   // LAPIC version
#define LAPIC_TPR         0x080   // Task Priority
#define LAPIC_EOI         0x0B0   // End of Interrupt
#define LAPIC_SVR         0x0F0   // Spurious Interrupt Vector Register
#define LAPIC_ESR         0x280   // Error Status
#define LAPIC_ICR_LO      0x300   // Interrupt Command (low 32 bits)
#define LAPIC_ICR_HI      0x310   // Interrupt Command (high 32 bits)
#define LAPIC_LVT_TIMER   0x320   // LVT Timer
#define LAPIC_LVT_THERMAL 0x330   // LVT Thermal Sensor
#define LAPIC_LVT_PERF    0x340   // LVT Performance Counter
#define LAPIC_LVT_LINT0   0x350   // LVT LINT0
#define LAPIC_LVT_LINT1   0x360   // LVT LINT1
#define LAPIC_LVT_ERROR   0x370   // LVT Error
#define LAPIC_TICR        0x380   // Timer Initial Count Register
#define LAPIC_TCCR        0x390   // Timer Current Count Register
#define LAPIC_TDCR        0x3E0   // Timer Divide Configuration Register

// SVR flags
#define LAPIC_SVR_ENABLE  (1 << 8)
#define LAPIC_SPURIOUS_VEC 0xFF

// LVT flags
#define LAPIC_LVT_MASKED  (1 << 16)
#define LAPIC_LVT_NMI     (0x4 << 8)   // NMI delivery mode

// PM Timer flags
#define LAPIC_TIMER_PERIODIC (1 << 17)
#define LAPIC_TIMER_ONESHOT (0)

#define ICR_INIT   0x00000500u   // delivery=INIT, edge, deassert
#define ICR_SIPI(page) (0x00000600u | ((page) & 0xFF))  // delivery=SIPI

#define AP_STACK_SIZE 4096 * 4

#define IPI_RESCHEDULE 0xF0

// ─── I/O APIC register indices ────────────────────────────────────────────────
//
// Access: write index to IOREGSEL (offset 0x00), read/write data at IOWIN (offset 0x10)

#define IOAPIC_REG_ID      0x00
#define IOAPIC_REG_VER     0x01   // bits[23:16] = max redirection entry (0-based)
#define IOAPIC_REG_ARB     0x02
#define IOAPIC_REDTBL(n)   (0x10 + 2*(n))        // low 32 bits of entry n
#define IOAPIC_REDTBL_HI(n)(0x10 + 2*(n) + 1)   // high 32 bits of entry n

// Redirection entry: low 32-bit word layout
// [7:0]  Vector
// [10:8] Delivery mode (000=Fixed, 100=NMI, 111=ExtINT)
// [11]   Destination mode (0=physical, 1=logical)
// [13]   Pin polarity (0=active-high, 1=active-low)
// [15]   Trigger mode (0=edge, 1=level)
// [16]   Mask (1=masked)
// High 32-bit word: [31:24] = destination APIC ID (physical mode)

#define IOAPIC_MASKED        (1 << 16)
#define IOAPIC_LEVEL         (1 << 15)
#define IOAPIC_ACTIVE_LOW    (1 << 13)
#define IOAPIC_DEST_LOGICAL  (1 << 11)

// ─── Global flag ─────────────────────────────────────────────────────────────

extern bool g_use_apic;

// ─── Public API ───────────────────────────────────────────────────────────────

// Called after acpi_init(). Maps MMIO, enables LAPIC, configures I/O APIC.
void apic_init(void);

// Called from interrupt_handler instead of pic_send_eoi when g_use_apic is set.
void lapic_send_eoi(void);

// Mask/unmask a GSI on the I/O APIC (by ISA IRQ number).
void ioapic_mask_irq(uint8_t isa_irq);
void ioapic_unmask_irq(uint8_t isa_irq);

// Returns the GSI for a given ISA IRQ (consulting the ISO table).
uint32_t acpi_irq_to_gsi(uint8_t isa_irq);

extern uint32_t lapic_read(uint32_t reg);

extern void lapic_write(uint32_t reg, uint32_t val);

extern void lapic_send_init(uint8_t apic_id);

extern void lapic_send_sipi(uint8_t apic_id, uint8_t page);

extern void lapic_send_ipi(uint8_t target_apic_id, uint8_t vector);
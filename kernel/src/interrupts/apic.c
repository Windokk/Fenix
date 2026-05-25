#include "interrupts/apic.h"
#include "memory/paging_pages.h"
#include "stdio.h"

bool g_use_apic = false;

// init_paging_apic is implemented in paging_pages.c but not exported via the
// header (to avoid the static-array duplication problem with paging_pages.h).
extern void init_paging_apic(uint64_t lapic_phys,
                              uint64_t *ioapic_phys, uint32_t ioapic_count);

// ─── LAPIC register access ────────────────────────────────────────────────────

static inline uint32_t lapic_read(uint32_t reg) {
    return *(volatile uint32_t *)(LAPIC_VIRT_BASE + reg);
}

static inline void lapic_write(uint32_t reg, uint32_t val) {
    *(volatile uint32_t *)(LAPIC_VIRT_BASE + reg) = val;
}

void lapic_send_eoi(void) {
    lapic_write(LAPIC_EOI, 0);
}

// ─── I/O APIC register access ────────────────────────────────────────────────
//
// The I/O APIC is accessed via a 2-register window:
//   IOREGSEL @ base+0x00  (write: register index)
//   IOWIN    @ base+0x10  (read/write: data)

static volatile uint32_t *ioapic_base_for(uint32_t idx) {
    return (volatile uint32_t *)(IOAPIC_VIRT_BASE + idx * IOAPIC_VIRT_STRIDE);
}

static uint32_t ioapic_read(volatile uint32_t *base, uint8_t reg) {
    base[0] = reg;    // IOREGSEL at offset 0x00  (index 0 of uint32_t array)
    return base[4];   // IOWIN    at offset 0x10  (index 4 of uint32_t array)
}

static void ioapic_write(volatile uint32_t *base, uint8_t reg, uint32_t val) {
    base[0] = reg;
    base[4] = val;
}

// ─── IRQ → GSI lookup via ACPI ISO table ─────────────────────────────────────

uint32_t acpi_irq_to_gsi(uint8_t isa_irq) {
    for (uint32_t i = 0; i < g_acpi.iso_count; i++) {
        if (g_acpi.isos[i].irq == isa_irq)
            return g_acpi.isos[i].gsi;
    }
    return isa_irq;  // default: 1:1 mapping
}

// Returns the ISO flags for a given ISA IRQ (0 if no override).
static uint16_t iso_flags_for(uint8_t isa_irq) {
    for (uint32_t i = 0; i < g_acpi.iso_count; i++) {
        if (g_acpi.isos[i].irq == isa_irq)
            return g_acpi.isos[i].flags;
    }
    return ISO_POLARITY_CONFORM | ISO_TRIGGER_CONFORM;
}

// Returns which IOAPIC owns a given GSI and its local index within that IOAPIC.
static bool gsi_to_ioapic(uint32_t gsi,
                           uint32_t *out_ioapic_idx, uint8_t *out_local) {
    for (uint32_t i = 0; i < g_acpi.ioapic_count; i++) {
        uint32_t base  = g_acpi.ioapics[i].gsi_base;
        volatile uint32_t *b = ioapic_base_for(i);
        uint8_t max = (uint8_t)((ioapic_read(b, IOAPIC_REG_VER) >> 16) & 0xFF) + 1;
        if (gsi >= base && gsi < base + max) {
            *out_ioapic_idx = i;
            *out_local      = (uint8_t)(gsi - base);
            return true;
        }
    }
    return false;
}

// ─── Redirection entry helpers ────────────────────────────────────────────────

static void ioapic_set_redirect(volatile uint32_t *base, uint8_t local_idx,
                                 uint8_t vector, uint8_t polarity, uint8_t trigger,
                                 uint8_t dest_apic_id, bool masked) {
    uint32_t lo = (uint32_t)vector
                | ((polarity & 1) << 13)   // active-low if 1
                | ((trigger  & 1) << 15)   // level if 1
                | (masked ? IOAPIC_MASKED : 0);
    uint32_t hi = (uint32_t)dest_apic_id << 24;

    ioapic_write(base, IOAPIC_REDTBL(local_idx),    lo);
    ioapic_write(base, IOAPIC_REDTBL_HI(local_idx), hi);
}

// ─── Public: IRQ masking via I/O APIC ────────────────────────────────────────

void ioapic_mask_irq(uint8_t isa_irq) {
    uint32_t gsi = acpi_irq_to_gsi(isa_irq);
    uint32_t io_idx; uint8_t local;
    if (!gsi_to_ioapic(gsi, &io_idx, &local)) return;

    volatile uint32_t *base = ioapic_base_for(io_idx);
    uint32_t lo = ioapic_read(base, IOAPIC_REDTBL(local));
    ioapic_write(base, IOAPIC_REDTBL(local), lo | IOAPIC_MASKED);
}

void ioapic_unmask_irq(uint8_t isa_irq) {
    uint32_t gsi = acpi_irq_to_gsi(isa_irq);
    uint32_t io_idx; uint8_t local;
    if (!gsi_to_ioapic(gsi, &io_idx, &local)) return;

    volatile uint32_t *base = ioapic_base_for(io_idx);
    uint32_t lo = ioapic_read(base, IOAPIC_REDTBL(local));
    ioapic_write(base, IOAPIC_REDTBL(local), lo & ~IOAPIC_MASKED);
}

// ─── LAPIC initialization ─────────────────────────────────────────────────────

static void lapic_init(void) {
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

    printf("[APIC] LAPIC enabled  ID=0x%X  version=0x%X\n",
           (lapic_read(LAPIC_ID) >> 24) & 0xFF,
           lapic_read(LAPIC_VER) & 0xFF);
}

// ─── I/O APIC initialization ──────────────────────────────────────────────────

static void ioapic_init_one(uint32_t idx) {
    volatile uint32_t *base = ioapic_base_for(idx);
    uint32_t gsi_base = g_acpi.ioapics[idx].gsi_base;

    uint32_t ver       = ioapic_read(base, IOAPIC_REG_VER);
    uint8_t  max_entry = (uint8_t)((ver >> 16) & 0xFF) + 1;

    printf("[APIC] IOAPIC #%u  %u entries  GSI base %u\n",
           idx, max_entry, gsi_base);

    // Mask every entry first
    for (uint8_t i = 0; i < max_entry; i++) {
        ioapic_write(base, IOAPIC_REDTBL(i),    IOAPIC_MASKED);
        ioapic_write(base, IOAPIC_REDTBL_HI(i), 0);
    }

    // Route ISA IRQs 0-15 → IDT vectors 32-47, consulting the ISO table
    for (uint8_t isa_irq = 0; isa_irq < 16; isa_irq++) {
        uint32_t gsi     = acpi_irq_to_gsi(isa_irq);
        uint16_t fl      = iso_flags_for(isa_irq);

        // Skip GSIs not handled by this IOAPIC
        if (gsi < gsi_base || gsi >= gsi_base + max_entry) continue;

        uint8_t local = (uint8_t)(gsi - gsi_base);
        uint8_t vector = 32 + isa_irq;  // same IDT vectors as the legacy PIC

        // ISA default: active-high, edge-triggered.
        // ISO override: use the flags from the MADT.
        uint8_t polarity = ((fl & ISO_POLARITY_MASK) == ISO_POLARITY_LOW)  ? 1 : 0;
        uint8_t trigger  = ((fl & ISO_TRIGGER_MASK)  == ISO_TRIGGER_LEVEL) ? 1 : 0;

        ioapic_set_redirect(base, local, vector, polarity, trigger,
                            0 /* CPU 0 */, true /* start masked */);
    }
}

// ─── Public: full APIC bringup ────────────────────────────────────────────────

void apic_init(void) {
    if (!g_acpi.initialized) {
        printf("[APIC] Skipped: ACPI not initialized\n");
        return;
    }
    if (g_acpi.ioapic_count == 0) {
        printf("[APIC] Skipped: no I/O APIC found in MADT\n");
        return;
    }

    // Disable legacy 8259 PICs by masking all lines.
    // The PICs stay powered; they just never deliver interrupts.
    // Device handlers that still call pic_send_eoi() are harmless
    // (they write to the PIC command port, which is a no-op when masked).
    outb(0x21, 0xFF);
    outb(0xA1, 0xFF);

    // Build array of physical IOAPIC addresses for the pager
    uint64_t ioapic_phys[ACPI_MAX_IOAPICS];
    for (uint32_t i = 0; i < g_acpi.ioapic_count; i++)
        ioapic_phys[i] = g_acpi.ioapics[i].phys_addr;

    // Map LAPIC + IOAPIC registers into the kernel's virtual address space
    init_paging_apic(g_acpi.lapic_phys, ioapic_phys, g_acpi.ioapic_count);

    lapic_init();

    for (uint32_t i = 0; i < g_acpi.ioapic_count; i++)
        ioapic_init_one(i);

    g_use_apic = true;

    // Unmask the IRQs actually used by existing drivers
    ioapic_unmask_irq(1);   // keyboard
    ioapic_unmask_irq(8);   // RTC
    ioapic_unmask_irq(12);  // PS/2 mouse

    printf("[APIC] Active — LAPIC + %u IOAPIC(s) configured\n",
           g_acpi.ioapic_count);
}

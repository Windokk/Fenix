#pragma once

#include "common.h"
#include "drivers/serial.h"

#include "uacpi/acpi.h"

#define ACPI_MAX_IOAPICS  4
#define ACPI_MAX_ISOS     24
#define ACPI_MAX_MCFG     4

// ISO polarity/trigger flag values
#define ISO_POLARITY_MASK     0x3
#define ISO_POLARITY_CONFORM  0x0
#define ISO_POLARITY_HIGH     0x1
#define ISO_POLARITY_LOW      0x3
#define ISO_TRIGGER_MASK      0xC
#define ISO_TRIGGER_CONFORM   0x0
#define ISO_TRIGGER_EDGE      0x4
#define ISO_TRIGGER_LEVEL     0xC

typedef struct {
    uint8_t  ioapic_id;
    uint64_t phys_addr;
    uint32_t gsi_base;
} acpi_ioapic_info_t;

typedef struct {
    uint8_t  irq;
    uint32_t gsi;
    uint16_t flags;
} acpi_iso_t;

typedef struct {
    bool     initialized;

    // From MADT
    uint64_t lapic_phys;
    uint32_t ioapic_count;
    acpi_ioapic_info_t ioapics[ACPI_MAX_IOAPICS];
    uint32_t iso_count;
    acpi_iso_t isos[ACPI_MAX_ISOS];

    // From FADT - PM1 control (shutdown/sleep)
    uint16_t pm1a_cnt_port;
    uint16_t pm1b_cnt_port;   // 0 if absent

    // From FADT - PM1 event block (SCI fixed events)
    uint32_t pm1a_evt_blk;
    uint32_t pm1b_evt_blk;   // 0 if absent
    uint8_t  pm1_evt_len;

    // From FADT - reset
    uint8_t  reset_space;     // 0=MMIO, 1=I/O port
    uint64_t reset_addr;
    uint8_t  reset_value;
    bool     reset_valid;

    // From FADT - PM Timer
    uint16_t pm_tmr_port;
    bool     pm_tmr_32bit;

    uint16_t sci_irq;         // GSI of the ACPI SCI interrupt

    // HPET
    bool     hpet_found;
    uint64_t hpet_phys;
    uint16_t hpet_min_tick;

    // PCIe ECAM (from MCFG)
    struct acpi_mcfg_allocation mcfg[ACPI_MAX_MCFG];
    uint32_t     mcfg_count;
} acpi_state_t;

extern acpi_state_t g_acpi;

/* Physical address of the RSDP — set during acpi_init(), used by uACPI. */
extern uint64_t g_rsdp_phys;

// ─── Public API ───────────────────────────────────────────────────────────────

// Call after init_paging(). mbi_phys is the physical pointer passed by GRUB.
void acpi_init(void *mbi_phys);

// Enter sleep state Sx (0–5). Returns false if the state is unavailable.
// acpi_enter_sleep(5) is equivalent to acpi_shutdown().
bool acpi_enter_sleep(uint8_t sx);

void acpi_shutdown(void);
void acpi_reboot(void);

// Called from the SCI IRQ handler to process PM1 fixed events.
void acpi_sci_handle(void);

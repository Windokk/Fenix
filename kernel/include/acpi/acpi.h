#pragma once

#include "common.h"
#include "drivers/serial.h"

// ─── ACPI table structures ────────────────────────────────────────────────────

typedef struct {
    char     signature[4];
    uint32_t length;
    uint8_t  revision;
    uint8_t  checksum;
    char     oem_id[6];
    char     oem_table_id[8];
    uint32_t oem_revision;
    uint32_t creator_id;
    uint32_t creator_revision;
} __attribute__((packed)) acpi_sdt_header_t;

typedef struct {
    char     signature[8];   // "RSD PTR "
    uint8_t  checksum;
    char     oem_id[6];
    uint8_t  revision;       // 0 = ACPI 1.0, 2 = ACPI 2.0+
    uint32_t rsdt_address;
    // ACPI 2.0+ fields:
    uint32_t length;
    uint64_t xsdt_address;
    uint8_t  extended_checksum;
    uint8_t  reserved[3];
} __attribute__((packed)) acpi_rsdp_t;

typedef struct {
    acpi_sdt_header_t header;
    uint32_t          entries[0];  // physical addresses of SDTs (32-bit)
} __attribute__((packed)) acpi_rsdt_t;

typedef struct {
    acpi_sdt_header_t header;
    uint64_t          entries[0];  // physical addresses of SDTs (64-bit)
} __attribute__((packed)) acpi_xsdt_t;

// Generic Address Structure (used in FADT for reset register, etc.)
typedef struct {
    uint8_t  address_space;  // 0=MMIO, 1=I/O port, 2=PCI config
    uint8_t  bit_width;
    uint8_t  bit_offset;
    uint8_t  access_size;
    uint64_t address;
} __attribute__((packed)) acpi_gas_t;

// Fixed ACPI Description Table — only the fields we actually need
typedef struct {
    acpi_sdt_header_t header;
    uint32_t firmware_ctrl;
    uint32_t dsdt;
    uint8_t  _res1;
    uint8_t  preferred_pm_profile;
    uint16_t sci_interrupt;
    uint32_t smi_cmd;
    uint8_t  acpi_enable;
    uint8_t  acpi_disable;
    uint8_t  s4bios_req;
    uint8_t  pstate_cnt;
    uint32_t pm1a_event_blk;
    uint32_t pm1b_event_blk;
    uint32_t pm1a_cnt_blk;
    uint32_t pm1b_cnt_blk;
    uint32_t pm2_cnt_blk;
    uint32_t pm_tmr_blk;
    uint32_t gpe0_blk;
    uint32_t gpe1_blk;
    uint8_t  pm1_evt_len;
    uint8_t  pm1_cnt_len;
    uint8_t  pm2_cnt_len;
    uint8_t  pm_tmr_len;
    uint8_t  gpe0_blk_len;
    uint8_t  gpe1_blk_len;
    uint8_t  gpe1_base;
    uint8_t  cst_cnt;
    uint16_t p_lvl2_lat;
    uint16_t p_lvl3_lat;
    uint16_t flush_size;
    uint16_t flush_stride;
    uint8_t  duty_offset;
    uint8_t  duty_width;
    uint8_t  day_alrm;
    uint8_t  mon_alrm;
    uint8_t  century;
    uint16_t iapc_boot_arch;
    uint8_t  _res2;
    uint32_t flags;
    acpi_gas_t reset_reg;
    uint8_t  reset_value;
    uint8_t  _res3[3];
    uint64_t x_firmware_ctrl;
    uint64_t x_dsdt;
} __attribute__((packed)) acpi_fadt_t;

#define FADT_FLAG_RESET_REG_VALID  (1 << 10)

// ─── MADT (Multiple APIC Description Table) ───────────────────────────────────

typedef struct {
    acpi_sdt_header_t header;
    uint32_t lapic_address;
    uint32_t flags;
} __attribute__((packed)) acpi_madt_t;

#define MADT_FLAG_PCAT_COMPAT  (1 << 0)  // legacy 8259 PICs present

typedef struct {
    uint8_t type;
    uint8_t length;
} __attribute__((packed)) madt_entry_t;

#define MADT_TYPE_LAPIC      0
#define MADT_TYPE_IOAPIC     1
#define MADT_TYPE_ISO        2   // Interrupt Source Override
#define MADT_TYPE_LAPIC_NMI  4
#define MADT_TYPE_LAPIC_OVR  5   // Local APIC address override (64-bit)
#define MADT_TYPE_X2APIC     9

typedef struct {
    madt_entry_t hdr;
    uint8_t  acpi_id;
    uint8_t  apic_id;
    uint32_t flags;              // bit 0: processor enabled
} __attribute__((packed)) madt_lapic_t;

typedef struct {
    madt_entry_t hdr;
    uint8_t  ioapic_id;
    uint8_t  _res;
    uint32_t ioapic_address;    // physical address of I/O APIC registers
    uint32_t gsi_base;          // first GSI handled by this I/O APIC
} __attribute__((packed)) madt_ioapic_t;

typedef struct {
    madt_entry_t hdr;
    uint8_t  bus;               // always 0 (ISA)
    uint8_t  irq;               // source ISA IRQ
    uint32_t gsi;               // target Global System Interrupt
    uint16_t flags;             // bits[1:0]=polarity, bits[3:2]=trigger
} __attribute__((packed)) madt_iso_t;

typedef struct {
    madt_entry_t hdr;
    uint8_t  acpi_id;           // 0xFF = all processors
    uint16_t flags;
    uint8_t  lint;              // LINT pin: 0 or 1
} __attribute__((packed)) madt_lapic_nmi_t;

// ISO polarity/trigger flag values
#define ISO_POLARITY_MASK     0x3
#define ISO_POLARITY_CONFORM  0x0
#define ISO_POLARITY_HIGH     0x1
#define ISO_POLARITY_LOW      0x3
#define ISO_TRIGGER_MASK      0xC
#define ISO_TRIGGER_CONFORM   0x0
#define ISO_TRIGGER_EDGE      0x4
#define ISO_TRIGGER_LEVEL     0xC

// ─── Global ACPI state ────────────────────────────────────────────────────────

#define ACPI_MAX_IOAPICS  4
#define ACPI_MAX_ISOS     24

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

    // From FADT — power management
    uint16_t pm1a_cnt_port;
    uint16_t pm1b_cnt_port;   // 0 if absent
    uint16_t slp_typa;        // S5 sleep type for PM1a (pre-shifted << 10)
    uint16_t slp_typb;        // S5 sleep type for PM1b
    bool     s5_found;

    // From FADT — reset
    uint8_t  reset_space;     // 0=MMIO, 1=I/O port
    uint64_t reset_addr;
    uint8_t  reset_value;
    bool     reset_valid;

    uint16_t sci_irq;
} acpi_state_t;

extern acpi_state_t g_acpi;

// ─── Public API ───────────────────────────────────────────────────────────────

// Call after init_paging(). mbi_phys is the physical pointer passed by GRUB.
void acpi_init(void *mbi_phys);

void acpi_shutdown(void);
void acpi_reboot(void);

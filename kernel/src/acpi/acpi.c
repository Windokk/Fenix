#include "acpi/acpi.h"
#include "multiboot2.h"
#include "memory/paging_pages.h"
#include "stdio.h"
#include "string.h"
#include "acpi/uacpi_glue.h"
#include <uacpi/sleep.h>

acpi_state_t g_acpi = {0};
uint64_t     g_rsdp_phys             = 0;
uint64_t     g_acpi_table_phys_base  = 0;

// ─── Multiboot2 tag structures for ACPI ──────────────────────────────────────

typedef struct {
    uint32_t type;
    uint32_t size;
    uint8_t  rsdp[0];
} __attribute__((packed)) mb2_tag_acpi_t;

typedef struct {
    uint32_t type;
    uint32_t size;
    uint8_t  tags[0];
} mb2_info_t;

// ─── Physical => virtual translation for ACPI tables ──────────────────────────

static inline void *acpi_phys_to_virt(uint64_t phys) {
    if (phys < (8ULL << 20))
        return (void *)TO_VMA_U64(phys);
    return (void *)(ACPI_TABLE_BASE + (phys - g_acpi_table_phys_base));
}

// ─── Validation ───────────────────────────────────────────────────────────────

static bool acpi_checksum(const void *data, uint32_t len) {
    const uint8_t *p = (const uint8_t *)data;
    uint8_t sum = 0;
    for (uint32_t i = 0; i < len; i++) sum += p[i];
    return sum == 0;
}

static bool acpi_validate_sdt(acpi_sdt_header_t *hdr) {
    return acpi_checksum(hdr, hdr->length);
}

// ─── Table lookup ─────────────────────────────────────────────────────────────

static bool g_use_xsdt;
static void *g_root_table_virt;

static acpi_sdt_header_t *acpi_find_table(const char sig[4]) {
    if (!g_root_table_virt) return NULL;

    if (g_use_xsdt) {
        acpi_xsdt_t *xsdt  = (acpi_xsdt_t *)g_root_table_virt;
        uint32_t     count = (xsdt->header.length - sizeof(acpi_sdt_header_t))
                             / sizeof(uint64_t);
        for (uint32_t i = 0; i < count; i++) {
            acpi_sdt_header_t *hdr =
                (acpi_sdt_header_t *)acpi_phys_to_virt(xsdt->entries[i]);
            if (memcmp(hdr->signature, sig, 4) == 0 && acpi_validate_sdt(hdr))
                return hdr;
        }
    } else {
        acpi_rsdt_t *rsdt  = (acpi_rsdt_t *)g_root_table_virt;
        uint32_t     count = (rsdt->header.length - sizeof(acpi_sdt_header_t))
                             / sizeof(uint32_t);
        for (uint32_t i = 0; i < count; i++) {
            acpi_sdt_header_t *hdr =
                (acpi_sdt_header_t *)acpi_phys_to_virt((uint64_t)rsdt->entries[i]);
            if (memcmp(hdr->signature, sig, 4) == 0 && acpi_validate_sdt(hdr))
                return hdr;
        }
    }
    return NULL;
}

// ─── FADT ─────────────────────────────────────────────────────────────────────

static void acpi_parse_fadt(void) {
    acpi_fadt_t *fadt = (acpi_fadt_t *)acpi_find_table("FACP");
    if (!fadt) { printf("[ACPI] FADT not found\n"); return; }

    g_acpi.sci_irq       = fadt->sci_interrupt;
    g_acpi.pm1a_cnt_port = (uint16_t)fadt->pm1a_cnt_blk;
    g_acpi.pm1b_cnt_port = (uint16_t)fadt->pm1b_cnt_blk;

    g_acpi.pm1a_evt_blk  = fadt->pm1a_event_blk;
    g_acpi.pm1b_evt_blk  = fadt->pm1b_event_blk;
    g_acpi.pm1_evt_len   = fadt->pm1_evt_len;

    g_acpi.pm_tmr_port   = fadt->pm_tmr_blk;
    g_acpi.pm_tmr_32bit  = fadt->pm_tmr_len == 4;
    
    if (fadt->header.revision >= 2 && (fadt->flags & FADT_FLAG_RESET_REG_VALID)) {
        g_acpi.reset_space = fadt->reset_reg.address_space;
        g_acpi.reset_addr  = fadt->reset_reg.address;
        g_acpi.reset_value = fadt->reset_value;
        g_acpi.reset_valid = true;
    }

    // Enable ACPI mode if the firmware hasn't done so yet
    if (fadt->smi_cmd && fadt->acpi_enable) {
        if (!(inw(g_acpi.pm1a_cnt_port) & 0x1)) {
            outb((uint16_t)fadt->smi_cmd, fadt->acpi_enable);
            for (int t = 0; t < 3000; t++) {
                if (inw(g_acpi.pm1a_cnt_port) & 0x1) break;
            }
        }
    }

    printf("[ACPI] FADT: PM1a_CNT=0x%X PM1b_CNT=0x%X PM1a_EVT=0x%X SCI_IRQ=%u\n",
           g_acpi.pm1a_cnt_port, g_acpi.pm1b_cnt_port,
           g_acpi.pm1a_evt_blk,  g_acpi.sci_irq);
}

// ─── MADT ─────────────────────────────────────────────────────────────────────

static void acpi_parse_madt(void) {
    acpi_madt_t *madt = (acpi_madt_t *)acpi_find_table("APIC");
    if (!madt) { printf("[ACPI] MADT not found — APIC unavailable\n"); return; }

    g_acpi.lapic_phys = (uint64_t)madt->lapic_address;

    uint8_t *entry = (uint8_t *)madt + sizeof(acpi_madt_t);
    uint8_t *end   = (uint8_t *)madt + madt->header.length;

    while (entry < end) {
        madt_entry_t *e = (madt_entry_t *)entry;

        switch (e->type) {
        case MADT_TYPE_LAPIC: {
            madt_lapic_t *lapic = (madt_lapic_t *)e;
            if (lapic->flags & 1)
                printf("[ACPI] CPU: ACPI_ID=%u APIC_ID=%u\n",
                       lapic->acpi_id, lapic->apic_id);
            break;
        }
        case MADT_TYPE_IOAPIC: {
            madt_ioapic_t *io = (madt_ioapic_t *)e;
            if (g_acpi.ioapic_count < ACPI_MAX_IOAPICS) {
                acpi_ioapic_info_t *inf = &g_acpi.ioapics[g_acpi.ioapic_count++];
                inf->ioapic_id = io->ioapic_id;
                inf->phys_addr = (uint64_t)io->ioapic_address;
                inf->gsi_base  = io->gsi_base;
                printf("[ACPI] IOAPIC #%u @ phys 0x%X  GSI base %u\n",
                       io->ioapic_id, io->ioapic_address, io->gsi_base);
            }
            break;
        }
        case MADT_TYPE_ISO: {
            madt_iso_t *iso = (madt_iso_t *)e;
            if (g_acpi.iso_count < ACPI_MAX_ISOS) {
                acpi_iso_t *s = &g_acpi.isos[g_acpi.iso_count++];
                s->irq   = iso->irq;
                s->gsi   = iso->gsi;
                s->flags = iso->flags;
                printf("[ACPI] ISO: IRQ %u -> GSI %u  flags=0x%X\n",
                       iso->irq, iso->gsi, iso->flags);
            }
            break;
        }
        case MADT_TYPE_LAPIC_OVR: {
            uint64_t *addr = (uint64_t *)(entry + 4);
            g_acpi.lapic_phys = *addr;
            printf("[ACPI] LAPIC address override: 0x%X\n",
                   (uint32_t)g_acpi.lapic_phys);
            break;
        }
        default: break;
        }

        entry += e->length;
    }

    printf("[ACPI] LAPIC @ phys 0x%X  %u IOAPIC(s)  %u ISO(s)\n",
           (uint32_t)g_acpi.lapic_phys, g_acpi.ioapic_count, g_acpi.iso_count);
}

// ─── HPET ─────────────────────────────────────────────────────────────────────

static void acpi_parse_hpet(void) {
    acpi_hpet_t *hpet = (acpi_hpet_t *)acpi_find_table("HPET");
    if (!hpet) { printf("[ACPI] HPET not found\n"); return; }

    g_acpi.hpet_found    = true;
    g_acpi.hpet_phys     = hpet->base_address.address;
    g_acpi.hpet_min_tick = hpet->minimum_tick;

    printf("[ACPI] HPET #%u @ phys 0x%X  min_tick=%u\n",
           hpet->hpet_number, (uint32_t)g_acpi.hpet_phys, hpet->minimum_tick);
}

// ─── MCFG ─────────────────────────────────────────────────────────────────────

static void acpi_parse_mcfg(void) {
    acpi_mcfg_t *hdr = (acpi_mcfg_t *)acpi_find_table("MCFG");
    if (!hdr) { printf("[ACPI] MCFG not found\n"); return; }

    uint32_t     count   = (hdr->header.length - sizeof(acpi_mcfg_t))
                           / sizeof(mcfg_alloc_t);
    mcfg_alloc_t *entries = (mcfg_alloc_t *)((uint8_t *)hdr + sizeof(acpi_mcfg_t));

    for (uint32_t i = 0; i < count && i < ACPI_MAX_MCFG; i++) {
        g_acpi.mcfg[g_acpi.mcfg_count++] = entries[i];
        printf("[ACPI] MCFG: seg=%u bus=%u-%u @ 0x%X\n",
               entries[i].pci_segment, entries[i].start_bus, entries[i].end_bus,
               (uint32_t)entries[i].base_address);
    }
}

// ─── Public: init ─────────────────────────────────────────────────────────────

void acpi_init(void *mbi_phys) {
    printf("[ACPI] Initializing...\n");

    init_paging_mmio_pool();

    mb2_info_t *mbi = (mb2_info_t *)TO_VMA_U64((uint64_t)mbi_phys);

    acpi_rsdp_t *rsdp = NULL;

    uint8_t *tag = mbi->tags;
    while (1) {
        uint32_t type = *(uint32_t *)tag;
        uint32_t size = *(uint32_t *)(tag + 4);
        if (type == 0) break;

        if (type == 15 || (type == 14 && !rsdp)) {
            mb2_tag_acpi_t *acpi_tag = (mb2_tag_acpi_t *)tag;
            rsdp = (acpi_rsdp_t *)acpi_tag->rsdp;
        }

        tag += (size + 7) & ~7;
    }

    if (!rsdp) { printf("[ACPI] RSDP not found\n"); return; }

    /* Store physical address for uACPI's uacpi_kernel_get_rsdp(). */
    g_rsdp_phys = TO_PHYS_U64(rsdp);

    if (memcmp(rsdp->signature, "RSD PTR ", 8) != 0) {
        printf("[ACPI] RSDP signature mismatch\n");
        return;
    }
    if (!acpi_checksum(rsdp, 20)) {
        printf("[ACPI] RSDP checksum failed\n");
        return;
    }

    char oem[7];
    memcpy(oem, rsdp->oem_id, 6);
    oem[6] = '\0';
    printf("[ACPI] RSDP revision=%u  OEM=%s\n", rsdp->revision, oem);

    uint64_t root_phys = (rsdp->revision >= 2 && rsdp->xsdt_address)
                         ? rsdp->xsdt_address
                         : (uint64_t)rsdp->rsdt_address;

    if (root_phys >= (8ULL << 20)) {
        g_acpi_table_phys_base = root_phys & ~(ACPI_TABLE_WINDOW - 1);
        init_paging_acpi_tables(g_acpi_table_phys_base);
        printf("[ACPI] Mapped table window: phys 0x%X -> virt 0x%X\n",
               (uint32_t)g_acpi_table_phys_base, (uint32_t)ACPI_TABLE_BASE);
    }

    if (rsdp->revision >= 2 && rsdp->xsdt_address) {
        acpi_xsdt_t *xsdt = (acpi_xsdt_t *)acpi_phys_to_virt(rsdp->xsdt_address);
        if (memcmp(xsdt->header.signature, "XSDT", 4) == 0
                && acpi_validate_sdt(&xsdt->header)) {
            g_use_xsdt        = true;
            g_root_table_virt = xsdt;
            printf("[ACPI] Using XSDT @ phys 0x%X\n",
                   (uint32_t)rsdp->xsdt_address);
        } else {
            printf("[ACPI] XSDT invalid, falling back to RSDT\n");
        }
    }

    if (!g_root_table_virt) {
        acpi_rsdt_t *rsdt =
            (acpi_rsdt_t *)acpi_phys_to_virt((uint64_t)rsdp->rsdt_address);
        if (memcmp(rsdt->header.signature, "RSDT", 4) == 0
                && acpi_validate_sdt(&rsdt->header)) {
            g_use_xsdt        = false;
            g_root_table_virt = rsdt;
            printf("[ACPI] Using RSDT @ phys 0x%X\n", rsdp->rsdt_address);
        } else {
            printf("[ACPI] RSDT invalid — aborting\n");
            return;
        }
    }

    acpi_parse_fadt();
    acpi_parse_madt();
    acpi_parse_hpet();
    acpi_parse_mcfg();

    g_acpi.initialized = true;
    printf("[ACPI] Ready\n");
}

// ─── Public: sleep / power management ────────────────────────────────────────

bool acpi_enter_sleep(uint8_t sx) {
    if (sx > 5) return false;

    if (!g_uacpi_ready) {
        printf("[ACPI] Cannot enter S%u: uACPI not ready\n", sx);
        return false;
    }

    uacpi_status st = uacpi_prepare_for_sleep_state((uacpi_sleep_state)sx);
    if (st != UACPI_STATUS_OK) {
        printf("[ACPI] S%u prepare failed: 0x%X\n", sx, (uint32_t)st);
        return false;
    }
    uacpi_enter_sleep_state((uacpi_sleep_state)sx);
    /* Should not return — if we do, the hardware rejected the sleep entry. */
    return false;
}

void acpi_shutdown(void) {
    if (!g_acpi.initialized) {
        printf("[ACPI] Cannot shutdown: not initialized\n");
        return;
    }
    acpi_enter_sleep(5);
}

void acpi_reboot(void) {
    if (g_uacpi_ready) {
        uacpi_reboot();
        /* Falls through only if uACPI reboot failed. */
    }

    if (g_acpi.reset_valid && g_acpi.reset_space == 1)
        outb((uint16_t)g_acpi.reset_addr, g_acpi.reset_value);

    // Keyboard controller reset fallback
    while (inb(0x64) & 0x02);
    outb(0x64, 0xFE);

    asm volatile("int $0xFF");  // triple-fault last resort
    for (;;) asm volatile("hlt");
}

// ─── Public: SCI fixed-event handler ─────────────────────────────────────────

void acpi_sci_handle(void) {
    if (!g_acpi.initialized || !g_acpi.pm1a_evt_blk) return;

    // Read PM1 status registers (offset 0 within each event block)
    uint16_t sts_a = inw((uint16_t)g_acpi.pm1a_evt_blk);
    uint16_t sts_b = g_acpi.pm1b_evt_blk
                     ? inw((uint16_t)g_acpi.pm1b_evt_blk) : 0;

    // Write 1 to clear the bits that fired (write-to-clear semantics)
    if (sts_a) outw((uint16_t)g_acpi.pm1a_evt_blk, sts_a);
    if (sts_b) outw((uint16_t)g_acpi.pm1b_evt_blk, sts_b);

    uint16_t sts = sts_a | sts_b;

    if (sts & PM1_STS_PWRBTN) {
        printf("[ACPI] Power button — shutting down\n");
        acpi_enter_sleep(5);
    }
    if (sts & PM1_STS_SLPBTN)
        printf("[ACPI] Sleep button pressed\n");
    if (sts & PM1_STS_RTC_STS)
        printf("[ACPI] RTC alarm\n");
}

#include "acpi/acpi.h"
#include "multiboot2.h"
#include "memory/paging_pages.h"
#include "stdio.h"
#include "string.h"

acpi_state_t g_acpi = {0};

// ─── Multiboot2 tag structures for ACPI ──────────────────────────────────────

typedef struct {
    uint32_t type;
    uint32_t size;
    uint8_t  rsdp[0];  // RSDP data embedded directly in the tag
} __attribute__((packed)) mb2_tag_acpi_t;

typedef struct {
    uint32_t type;
    uint32_t size;
    uint8_t  tags[0];
} mb2_info_t;

// ─── Physical → virtual translation for ACPI tables ──────────────────────────
//
// GRUB places ACPI tables near the top of RAM (e.g. ~128 MB on a 128 MB VM),
// well above the 8 MB that init_paging_kernel() maps.  init_paging_acpi_tables()
// creates a 16 MB window at ACPI_TABLE_BASE covering the tables; after that
// every SDT physical address translates cleanly through acpi_phys_to_virt().

static uint64_t g_acpi_table_phys_base = 0;

static inline void *acpi_phys_to_virt(uint64_t phys) {
    if (phys < (8ULL << 20))
        return (void *)TO_VMA_U64(phys);   // within the normal kernel mapping
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

// ─── DSDT: find _S5_ to get the S5 (power-off) sleep type values ─────────────

static void acpi_parse_s5(acpi_sdt_header_t *dsdt) {
    uint8_t *data = (uint8_t *)dsdt + sizeof(acpi_sdt_header_t);
    uint32_t len  = dsdt->length - sizeof(acpi_sdt_header_t);

    for (uint32_t i = 0; i + 4 < len; i++) {
        if (data[i]   != '_' || data[i+1] != 'S' ||
            data[i+2] != '5' || data[i+3] != '_') continue;

        bool name_ok = (i >= 1 && data[i-1] == 0x08)
                    || (i >= 2 && data[i-2] == 0x08 && data[i-1] == '\\');
        if (!name_ok) continue;

        if (i + 4 >= len || data[i+4] != 0x12) continue;

        uint8_t *pkg = data + i + 5;
        // Skip PkgLength: bits[7:6] = number of extra bytes
        pkg += ((*pkg >> 6) + 1);
        pkg++;  // skip element count

        if (*pkg == 0x0A) pkg++;
        g_acpi.slp_typa = (uint16_t)(*pkg++) << 10;

        if (*pkg == 0x0A) pkg++;
        g_acpi.slp_typb = (uint16_t)(*pkg++) << 10;

        g_acpi.s5_found = true;
        printf("[ACPI] _S5_ found: SLP_TYPa=0x%X SLP_TYPb=0x%X\n",
               g_acpi.slp_typa >> 10, g_acpi.slp_typb >> 10);
        return;
    }
    printf("[ACPI] _S5_ not found in DSDT\n");
}

// ─── FADT ─────────────────────────────────────────────────────────────────────

static void acpi_parse_fadt(void) {
    acpi_fadt_t *fadt = (acpi_fadt_t *)acpi_find_table("FACP");
    if (!fadt) { printf("[ACPI] FADT not found\n"); return; }

    g_acpi.sci_irq       = fadt->sci_interrupt;
    g_acpi.pm1a_cnt_port = (uint16_t)fadt->pm1a_cnt_blk;
    g_acpi.pm1b_cnt_port = (uint16_t)fadt->pm1b_cnt_blk;

    if (fadt->header.revision >= 2 && (fadt->flags & FADT_FLAG_RESET_REG_VALID)) {
        g_acpi.reset_space = fadt->reset_reg.address_space;
        g_acpi.reset_addr  = fadt->reset_reg.address;
        g_acpi.reset_value = fadt->reset_value;
        g_acpi.reset_valid = true;
    }

    // Enable ACPI mode if the firmware hasn't done so yet (SCI_EN = bit 0 of PM1a_CNT)
    if (fadt->smi_cmd && fadt->acpi_enable) {
        if (!(inp(g_acpi.pm1a_cnt_port) & 0x1)) {
            outb((uint16_t)fadt->smi_cmd, fadt->acpi_enable);
            for (int t = 0; t < 3000; t++) {
                if (inp(g_acpi.pm1a_cnt_port) & 0x1) break;
            }
        }
    }

    // Locate DSDT and search it for _S5_
    uint64_t dsdt_phys = (fadt->header.revision >= 2 && fadt->x_dsdt)
                         ? fadt->x_dsdt : (uint64_t)fadt->dsdt;
    if (dsdt_phys) {
        acpi_sdt_header_t *dsdt = (acpi_sdt_header_t *)acpi_phys_to_virt(dsdt_phys);
        if (memcmp(dsdt->signature, "DSDT", 4) == 0 && acpi_validate_sdt(dsdt))
            acpi_parse_s5(dsdt);
    }

    printf("[ACPI] FADT: PM1a=0x%X PM1b=0x%X SCI_IRQ=%u\n",
           g_acpi.pm1a_cnt_port, g_acpi.pm1b_cnt_port, g_acpi.sci_irq);
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

// ─── Public: init ─────────────────────────────────────────────────────────────

void acpi_init(void *mbi_phys) {
    printf("[ACPI] Initializing...\n");

    mb2_info_t *mbi = (mb2_info_t *)TO_VMA_U64((uint64_t)mbi_phys);

    acpi_rsdp_t *rsdp = NULL;

    uint8_t *tag = mbi->tags;
    while (1) {
        uint32_t type = *(uint32_t *)tag;
        uint32_t size = *(uint32_t *)(tag + 4);
        if (type == 0) break;

        // Prefer type 15 (ACPI 2.0 RSDP) over type 14 (ACPI 1.0 RSDP)
        if (type == 15 || (type == 14 && !rsdp)) {
            mb2_tag_acpi_t *acpi_tag = (mb2_tag_acpi_t *)tag;
            rsdp = (acpi_rsdp_t *)acpi_tag->rsdp;
        }

        tag += (size + 7) & ~7;
    }

    if (!rsdp) { printf("[ACPI] RSDP not found\n"); return; }

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
    printf("[ACPI] RSDP found  revision=%u  OEM=%s\n", rsdp->revision, oem);

    // ── Map the ACPI table region before touching any SDT pointer ────────────
    //
    // Determine which root table we'll use and its physical address, then map
    // the aligned 16 MB block that contains it (and almost certainly all child
    // tables too, since GRUB packs them contiguously).

    uint64_t root_phys = (rsdp->revision >= 2 && rsdp->xsdt_address)
                         ? rsdp->xsdt_address
                         : (uint64_t)rsdp->rsdt_address;

    if (root_phys >= (8ULL << 20)) {
        // Round down to a 16 MB boundary so the entire surrounding region is
        // covered by the single init_paging_acpi_tables() call.
        g_acpi_table_phys_base = root_phys & ~(ACPI_TABLE_WINDOW - 1);
        init_paging_acpi_tables(g_acpi_table_phys_base);
        printf("[ACPI] Mapped table window: phys 0x%X -> virt 0x%X\n",
               (uint32_t)g_acpi_table_phys_base, (uint32_t)ACPI_TABLE_BASE);
    }
    // If root_phys < 8 MB it falls inside the normal kernel mapping;
    // acpi_phys_to_virt() handles it with TO_VMA_U64, no extra mapping needed.

    // ── Locate and validate the root table ───────────────────────────────────

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

    g_acpi.initialized = true;
    printf("[ACPI] Ready\n");
}

// ─── Public: power management ─────────────────────────────────────────────────

void acpi_shutdown(void) {
    if (!g_acpi.initialized || !g_acpi.s5_found) {
        printf("[ACPI] Cannot shutdown: %s\n",
               !g_acpi.initialized ? "not initialized" : "_S5_ not found");
        return;
    }
    uint16_t slp_en = 1 << 13;
    outp(g_acpi.pm1a_cnt_port, g_acpi.slp_typa | slp_en);
    if (g_acpi.pm1b_cnt_port)
        outp(g_acpi.pm1b_cnt_port, g_acpi.slp_typb | slp_en);
    for (;;) asm volatile("hlt");
}

void acpi_reboot(void) {
    if (g_acpi.reset_valid && g_acpi.reset_space == 1)
        outb((uint16_t)g_acpi.reset_addr, g_acpi.reset_value);

    // Keyboard controller reset (universal fallback)
    while (inb(0x64) & 0x02);
    outb(0x64, 0xFE);

    asm volatile("int $0xFF");  // triple-fault last resort
    for (;;) asm volatile("hlt");
}

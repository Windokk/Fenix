#include "acpi/acpi.h"
#include "multiboot2.h"

#include "memory/paging_pages.h"

#include "stdio.h"
#include "string.h"

#include "acpi/uacpi_glue.h"

#include <uacpi/sleep.h>
#include <uacpi/event.h>
#include <uacpi/tables.h>

acpi_state_t g_acpi                  = {0};
uint64_t     g_rsdp_phys             = 0;
uint8_t*     g_rsdp_virt             = 0;

bool g_lapic_ready = false;

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

uacpi_interrupt_ret power_button_handler(uacpi_handle ctx) {
    (void)ctx;
    KLOG_INFO("ACPI", "Power button pressed : shutting down");
    acpi_enter_sleep(5);
    return UACPI_INTERRUPT_HANDLED;
}

// ─── Public: init ─────────────────────────────────────────────────────────────

void acpi_init(void *mbi_phys) {
    KLOG_INFO("ACPI", "Initializing...");

    init_paging_mmio_pool();

    mb2_info_t *mbi = (mb2_info_t *)TO_VMA_U64((uint64_t)mbi_phys);

    uint8_t *tag = mbi->tags;
    while (1) {
        uint32_t type = *(uint32_t *)tag;
        uint32_t size = *(uint32_t *)(tag + 4);
        if (type == 0) break;

        if (type == 15 || (type == 14 && g_rsdp_virt == NULL)) {
            mb2_tag_acpi_t *acpi_tag = (mb2_tag_acpi_t *)tag;
            g_rsdp_virt = acpi_tag->rsdp;
        }

        tag += (size + 7) & ~7;
    }

    if (g_rsdp_virt == 0) { KLOG_INFO("ACPI", "RSDP not found"); return; }

    /* Store physical address for uACPI's uacpi_kernel_get_rsdp() */
    g_rsdp_phys = TO_PHYS_U64(g_rsdp_virt);

    uacpi_status ust = uacpi_initialize(UACPI_FLAG_NO_ACPI_MODE);
    KLOG_INFO("uACPI", "initialize done");
    if (ust == UACPI_STATUS_OK) {
        KLOG_INFO("uACPI", "calling namespace_load()");
        ust = uacpi_namespace_load();
        KLOG_INFO("uACPI", "namespace_load() done");
        if (ust == UACPI_STATUS_OK) {
            KLOG_INFO("uACPI", "calling namespace_initialize()");
            ust = uacpi_namespace_initialize();
            KLOG_INFO("uACPI", "namespace_initialize() done");
            if (ust == UACPI_STATUS_OK) {
                g_uacpi_ready = true;
                KLOG_INFO("uACPI", "Full namespace initialized");
            } else {
                KLOG_INFO("uACPI", "namespace_initialize() failed: 0x%X", (uint32_t)ust);
                return;
            }
        } else {
            KLOG_INFO("uACPI", "namespace_load failed: 0x%X", (uint32_t)ust);
            return;
        }
    } else {
        KLOG_INFO("uACPI", "initialize failed: 0x%X", (uint32_t)ust);
        return;
    }

    uacpi_install_fixed_event_handler(
                    UACPI_FIXED_EVENT_POWER_BUTTON,
                    power_button_handler, UACPI_NULL);

    struct acpi_fadt *fadt;
    if(uacpi_table_fadt(&fadt) != UACPI_STATUS_OK){
        KLOG_INFO("ACPI", "FADT table parsing was unsuccessful : code %u",ust);
    }
    else
    {
        g_acpi.sci_irq = fadt->sci_int;
        g_acpi.pm1a_cnt_port = (uint16_t)fadt->pm1a_cnt_blk;
        g_acpi.pm1b_cnt_port = (uint16_t)fadt->pm1b_cnt_blk;
        g_acpi.pm1a_evt_blk = fadt->pm1a_evt_blk;
        g_acpi.pm1b_evt_blk = fadt->pm1b_evt_blk;
        g_acpi.pm1_evt_len = fadt->pm1_evt_len;
        g_acpi.pm_tmr_port = (uint16_t)fadt->pm_tmr_blk;
        g_acpi.pm_tmr_32bit = fadt->pm_tmr_len == 4;
        g_acpi.reset_space = fadt->reset_reg.address_space_id;
        g_acpi.reset_addr = fadt->reset_reg.address;
        g_acpi.reset_value = fadt->reset_value;
        g_acpi.reset_valid = fadt->flags & ACPI_RESET_REG_SUP;
    }

    uacpi_table tbl;
    ust = uacpi_table_find_by_signature("APIC", &tbl);
    if(ust != UACPI_STATUS_OK){
        KLOG_INFO("ACPI", "APIC table parsing was unsuccessful : code %u",ust);
    }
    else
    {    struct acpi_madt *madt = (struct acpi_madt *)tbl.hdr;

        g_acpi.lapic_phys = (uint64_t)madt->local_interrupt_controller_address;

        struct acpi_entry_hdr *e = (struct acpi_entry_hdr *)madt->entries;
        uint8_t *end  = (uint8_t *)madt + madt->hdr.length;
        while ((uint8_t *)e < end) {
            switch (e->type) {
                case ACPI_MADT_ENTRY_TYPE_IOAPIC: {
                    struct acpi_madt_ioapic *io = (struct acpi_madt_ioapic *)e;
                    if(g_acpi.ioapic_count < ACPI_MAX_IOAPICS){
                        g_acpi.ioapics[g_acpi.ioapic_count].ioapic_id = io->id;
                        g_acpi.ioapics[g_acpi.ioapic_count].gsi_base = (uint64_t)io->gsi_base;
                        g_acpi.ioapics[g_acpi.ioapic_count].phys_addr = io->address;
                        g_acpi.ioapic_count++;
                    }
                    break;
                }
                case ACPI_MADT_ENTRY_TYPE_INTERRUPT_SOURCE_OVERRIDE: {
                    struct acpi_madt_interrupt_source_override *iso = (struct acpi_madt_interrupt_source_override *)e;
                    if(g_acpi.iso_count < ACPI_MAX_ISOS){
                        g_acpi.isos[g_acpi.iso_count].gsi = iso->gsi;
                        g_acpi.isos[g_acpi.iso_count].flags = iso->flags;
                        g_acpi.isos[g_acpi.iso_count].irq = iso->source;
                        g_acpi.iso_count++;
                    }
                    break;
                }
                case ACPI_MADT_ENTRY_TYPE_LAPIC_ADDRESS_OVERRIDE: {
                    struct acpi_madt_lapic_address_override *ovr = (struct acpi_madt_lapic_address_override *)e;
                    g_acpi.lapic_phys = ovr->address;
                    break;
                }
                case ACPI_MADT_ENTRY_TYPE_LAPIC: {
                    struct acpi_madt_lapic *lapic = (struct acpi_madt_lapic *)e;
                    bool enabled = lapic->flags & ACPI_PIC_ENABLED;
                    bool can_enable = lapic->flags & ACPI_PIC_ONLINE_CAPABLE;
                    if ((enabled || can_enable) && g_acpi.cpu_count < ACPI_MAX_CPUS) {
                        g_acpi.cpus[g_acpi.cpu_count].apic_id = lapic->id;
                        g_acpi.cpus[g_acpi.cpu_count].uid     = lapic->uid;
                        g_acpi.cpu_count++;
                    }
                    break;
                }
                case ACPI_MADT_ENTRY_TYPE_LOCAL_X2APIC: {
                    struct acpi_madt_x2apic *x2 = (struct acpi_madt_x2apic *)e;
                    bool enabled = x2->flags & ACPI_PIC_ENABLED;
                    bool can_enable = x2->flags & ACPI_PIC_ONLINE_CAPABLE;
                    if ((enabled || can_enable) && x2->id != 0xFFFFFFFF
                        && g_acpi.cpu_count < ACPI_MAX_CPUS) {
                        g_acpi.cpus[g_acpi.cpu_count].apic_id = x2->id;
                        g_acpi.cpus[g_acpi.cpu_count].uid     = x2->uid;
                        g_acpi.cpu_count++;
                    }
                    break;
                }
            }
            e = (struct acpi_entry_hdr *)((uint8_t *)e + e->length);
        }
    }

    ust = uacpi_table_find_by_signature("HPET", &tbl);
    if(ust != UACPI_STATUS_OK){
        KLOG_INFO("ACPI", "HPET table parsing was unsuccessful : code %u",ust);
    }
    else{
        struct acpi_hpet *hpet = (struct acpi_hpet *)tbl.hdr;
        g_acpi.hpet_found = true;
        g_acpi.hpet_phys = hpet->address.address;
        g_acpi.hpet_min_tick = hpet->min_clock_tick;
    }


    ust = uacpi_table_find_by_signature("MCFG", &tbl);
    if(ust != UACPI_STATUS_OK){
        KLOG_INFO("ACPI", "MCFG table parsing was unsuccessful : code %u",ust);
    }
    else{
        struct acpi_mcfg *mcfg = (struct acpi_mcfg *)tbl.hdr;
        uint32_t count = (mcfg->hdr.length - sizeof(struct acpi_mcfg))
                        / sizeof(struct acpi_mcfg_allocation);

        for(uint32_t i = 0; i < (uint32_t)count; i++){
            if (i >= ACPI_MAX_MCFG) break;
            g_acpi.mcfg[i] = mcfg->entries[i];
        }

        g_acpi.mcfg_count = (count < ACPI_MAX_MCFG) ? count : ACPI_MAX_MCFG;
    }

    g_acpi.initialized = true;

    KLOG_INFO("ACPI", "Ready");
}

// ─── Public: sleep / power management ────────────────────────────────────────

bool acpi_enter_sleep(uint8_t sx) {
    if (sx > 5) return false;

    if (!g_uacpi_ready) {
        KLOG_INFO("ACPI", "Cannot enter S%u: uACPI not ready\n", sx);
        return false;
    }

    uacpi_status st = uacpi_prepare_for_sleep_state((uacpi_sleep_state)sx);
    if (st != UACPI_STATUS_OK) {
        KLOG_INFO("ACPI", "S%u prepare failed: 0x%X\n", sx, (uint32_t)st);
        return false;
    }
    uacpi_enter_sleep_state((uacpi_sleep_state)sx);
    /* Should not return — if we do, the hardware rejected the sleep entry. */
    return false;
}

void acpi_shutdown(void) {
    if (!g_acpi.initialized) {
        KLOG_INFO("ACPI", "Cannot shutdown: not initialized\n");
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

    if (sts & ACPI_PM1_STS_PWRBTN_STS_MASK) {
        KLOG_INFO("ACPI", "Power button — shutting down\n");
        acpi_enter_sleep(5);
    }
    if (sts & ACPI_PM1_STS_SLPBTN_STS_MASK)
        KLOG_INFO("ACPI", "Sleep button pressed\n");
    if (sts & ACPI_PM1_STS_RTC_STS_MASK)
        KLOG_INFO("ACPI", "RTC alarm\n");
}

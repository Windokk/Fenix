#include "drivers/hpet.h"

#include "acpi/acpi.h"

#include "memory/paging_pages.h"

#include <uacpi/tables.h>
#include <uacpi/acpi.h>

uint32_t freq_hz = 0;

volatile uint64_t *hpet_regs;

void hpet_init(void){
    if (!g_acpi.initialized) {
        KLOG_INFO("HPET","Skipped : ACPI not initialized");
        return;
    }

    uacpi_table hpet_table;

    if(uacpi_table_find_by_signature("HPET", &hpet_table) == UACPI_STATUS_OK){
        struct acpi_hpet *h = (struct acpi_hpet*)hpet_table.ptr;
        uint64_t hpet_phys = h->address.address;
        hpet_regs = (volatile uint64_t *)paging_map_mmio(hpet_phys, 4096);
        uint32_t counter_clk_period = (*hpet_regs >> 32);
        freq_hz = 1000000000000000ULL / counter_clk_period;
        volatile uint64_t *general_config_reg = hpet_regs+0x010/8;
        (*general_config_reg) |= 1;

        KLOG_INFO("HPET", "Correctly initialized HPET with frequency : %u",freq_hz);
    }
}

uint64_t hpet_read_counter(void){
    volatile uint64_t *counter_value_reg = hpet_regs+0x0F0/8;
    return *counter_value_reg;
}

uint64_t hpet_get_nanos(void){
    volatile uint64_t *counter_value_reg = hpet_regs+0x0F0/8;
    return *counter_value_reg * (freq_hz / 1000000);
}
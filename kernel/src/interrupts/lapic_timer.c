#include "interrupts/lapic_timer.h"

#include "acpi/acpi.h"

#include "interrupts/apic.h"

#include "stdio.h"

uint32_t lapic_ticks_per_ms = 0;

void lapic_timer_init(void){
    char buf[32];
    serial_print("[LAPIC] pm_tmr_port=0x");
    itoa(g_acpi.pm_tmr_port, buf, 16);
    serial_print(buf);
    serial_print("\n");

    if(g_acpi.pm_tmr_port != 0)
    {
        uint32_t lapic_ticks_per_ms_average = 0;

        for(uint8_t i = 0; i < 3; i++){
            lapic_write(LAPIC_TDCR, 0x03); // Set the effective divisor to 16
            lapic_write(LAPIC_LVT_TIMER, LAPIC_LVT_MASKED | 0xFF); // Mask the interrupt as it is not useful right now
            lapic_write(LAPIC_TICR, 0xFFFFFFFF); // Start countdown
            uint32_t pm_start = inl(g_acpi.pm_tmr_port);
            uint32_t pm_target = 35795;
            uint32_t mask = g_acpi.pm_tmr_32bit ? 0xFFFFFFFF : 0x00FFFFFF;
            while (((inl(g_acpi.pm_tmr_port) - pm_start) & mask) < pm_target)
                ; // spin wait
            uint32_t tccr = lapic_read(LAPIC_TCCR);
            uint32_t elapsed = 0xFFFFFFFF - tccr;   // LAPIC ticks counted during 10ms
            lapic_write(LAPIC_TICR, 0);
            lapic_ticks_per_ms_average += elapsed / 10;
        }

        lapic_ticks_per_ms = lapic_ticks_per_ms_average / 3;

        printf("[LAPIC] timer calibrated: %u ticks/ms\n", lapic_ticks_per_ms);

        lapic_write(LAPIC_LVT_TIMER, 0x40 | LAPIC_TIMER_PERIODIC);
        lapic_write(LAPIC_TICR, lapic_ticks_per_ms); 
    }
    else{
        /// @todo : fire an error/warning
    }
}

void lapic_timer_handle_irq(void){
}
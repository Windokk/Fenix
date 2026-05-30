#include "drivers/rtc.h"
#include "interrupts/pic.h"
#include "stdio.h"

// CMOS I/O ports
#define CMOS_ADDR  0x70
#define CMOS_DATA  0x71

// Disabling NMI while accessing CMOS prevents the NMI from corrupting a
// partially-written index register.
#define NMI_DISABLE 0x80

// CMOS register indices
#define RTC_SECONDS   0x00
#define RTC_MINUTES   0x02
#define RTC_HOURS     0x04
#define RTC_DAY       0x07
#define RTC_MONTH     0x08
#define RTC_YEAR      0x09
#define RTC_STATUS_A  0x0A
#define RTC_STATUS_B  0x0B
#define RTC_STATUS_C  0x0C  // must be read after every IRQ to re-arm it

// Status A bits
#define STATUS_A_UIP  (1 << 7)  // Update In Progress — don't read time while set

// Status B bits
#define STATUS_B_24HR    (1 << 1)  // 1 = 24h mode, 0 = 12h mode
#define STATUS_B_BINARY  (1 << 2)  // 1 = binary registers, 0 = BCD
#define STATUS_B_PIE     (1 << 6)  // Periodic Interrupt Enable

// Periodic interrupt rate: frequency = 32768 >> (rate - 1)
// Rate 13 => 8 Hz (125 ms/tick) — used as the preemption quantum
// Rate 15 => 2 Hz (500 ms/tick)
#define RTC_RATE  15

static uint64_t rtc_ticks = 0;

static uint8_t cmos_read(uint8_t reg) {
    outb(CMOS_ADDR, NMI_DISABLE | reg);
    return inb(CMOS_DATA);
}

static void cmos_write(uint8_t reg, uint8_t val) {
    outb(CMOS_ADDR, NMI_DISABLE | reg);
    outb(CMOS_DATA, val);
}

static uint8_t bcd_to_bin(uint8_t bcd) {
    return ((bcd >> 4) * 10) + (bcd & 0x0F);
}

void rtc_read_time(rtc_time_t *out) {
    // Spin until the RTC is not mid-update, otherwise reads can straddle
    // a tick boundary and return a torn value (e.g. 23:59:60).
    while (cmos_read(RTC_STATUS_A) & STATUS_A_UIP);

    uint8_t status_b  = cmos_read(RTC_STATUS_B);
    bool    is_binary = (status_b & STATUS_B_BINARY) != 0;
    bool    is_24hr   = (status_b & STATUS_B_24HR)   != 0;

    uint8_t hours_raw = cmos_read(RTC_HOURS);
    uint8_t pm_bit    = hours_raw & 0x80;   // only meaningful in 12h mode

    out->seconds = cmos_read(RTC_SECONDS);
    out->minutes = cmos_read(RTC_MINUTES);
    out->hours   = hours_raw & 0x7F;        // strip PM bit before BCD decode
    out->day     = cmos_read(RTC_DAY);
    out->month   = cmos_read(RTC_MONTH);
    out->year    = cmos_read(RTC_YEAR);

    if (!is_binary) {
        out->seconds = bcd_to_bin(out->seconds);
        out->minutes = bcd_to_bin(out->minutes);
        out->hours   = bcd_to_bin(out->hours);
        out->day     = bcd_to_bin(out->day);
        out->month   = bcd_to_bin(out->month);
        out->year    = bcd_to_bin(out->year);
    }

    // Convert 12h => 24h if necessary
    if (!is_24hr) {
        if (pm_bit && out->hours != 12)
            out->hours += 12;
        else if (!pm_bit && out->hours == 12)
            out->hours = 0;
    }
}

void rtc_handle_irq(void) {
    rtc_ticks++;
    
    // Reading Status C clears the interrupt flags inside the RTC.
    // Without this read the RTC will never fire another IRQ.
    cmos_read(RTC_STATUS_C);

    cursor_blink_tick();

    pic_send_eoi(PIC_CMOS_REALTIME_CLOCK_INTERRUPT);
}

uint64_t rtc_get_ticks(void) {
    return rtc_ticks;
}

void rtc_init(void) {
    // Program the periodic interrupt rate (bits 3:0 of Status A).
    // Preserve the top 4 bits (divider chain - do not touch them).
    uint8_t prev_a = cmos_read(RTC_STATUS_A);
    cmos_write(RTC_STATUS_A, (prev_a & 0xF0) | RTC_RATE);

    // Enable the Periodic Interrupt in Status B.
    uint8_t prev_b = cmos_read(RTC_STATUS_B);
    cmos_write(RTC_STATUS_B, prev_b | STATUS_B_PIE);

    // Clear any stale interrupt left over from before init.
    cmos_read(RTC_STATUS_C);

    // IRQ 8 lives on PIC2. PIC2 itself is gated behind IRQ 2 on PIC1
    // (already unmasked in kernel_premain), so we only need to unmask IRQ 8.
    pic_unmask_irq(PIC_CMOS_REALTIME_CLOCK_INTERRUPT);

    printf("RTC initialised at %d Hz\n", 32768 >> (RTC_RATE - 1));
}

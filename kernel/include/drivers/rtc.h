#ifndef __RTC_H
#define __RTC_H

#include "common.h"

typedef struct {
    uint8_t seconds;
    uint8_t minutes;
    uint8_t hours;
    uint8_t day;
    uint8_t month;
    uint8_t year;   // 0–99 (add 2000 for full year)
} rtc_time_t;

void     rtc_init(void);
void     rtc_read_time(rtc_time_t *out);
void     rtc_handle_irq(void);
uint64_t rtc_get_ticks(void);

#endif

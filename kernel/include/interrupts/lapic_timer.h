#pragma once

#include "common.h"

extern void lapic_timer_init(void);
extern void lapic_timer_handle_irq(void);
extern void wait_ms(int duration);

extern uint32_t lapic_ticks_per_ms;
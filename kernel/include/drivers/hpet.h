#pragma once

#include "common.h"

extern void hpet_init(void);

extern uint64_t hpet_read_counter(void);

extern uint64_t hpet_get_nanos(void);

extern uint32_t freq_hz;
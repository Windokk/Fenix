#pragma once

#include "common.h"

typedef enum {
    PROF_ISR,
    PROF_KMALLOC,
    PROF_KFREE,
    _PROF_COUNT_
} probe_id_t;

typedef struct profiling_probe_t{

    uint32_t count;
    uint64_t total_cycles;
    uint64_t max_cycles;
    uint64_t min_cycles;

} profiling_probe_t;

extern profiling_probe_t probes[_PROF_COUNT_];

// note : tsc can be not monotonic on smp.
uint64_t rdtsc();

void prof_record(probe_id_t id, uint64_t cycles_count);

void debug_prof_dump();
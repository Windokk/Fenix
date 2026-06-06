#include "profiling/profiling.h"

#include "common.h"
#include "stdio.h"

profiling_probe_t probes[_PROF_COUNT_] = {{ 0, 0, 0, UINT64_MAX},{ 0, 0, 0, UINT64_MAX},{ 0, 0, 0, UINT64_MAX}};

const char* probes_names[_PROF_COUNT_] = {
    "Interrupt subroutine",
    "Kmalloc",
    "Kfree"
};

uint64_t rdtsc()
{
    unsigned int tickl, tickh;
    asm volatile("rdtsc":"=a"(tickl),"=d"(tickh));
    return ((uint64_t)tickh << 32) | tickl;
}

void prof_record(probe_id_t id, uint64_t cycles_count)
{
    probes[(int)id].count ++;
    probes[(int)id].total_cycles += cycles_count;
    probes[(int)id].max_cycles = cycles_count > probes[(int)id].max_cycles ? cycles_count : probes[(int)id].max_cycles;
    probes[(int)id].min_cycles = cycles_count < probes[(int)id].min_cycles ? cycles_count : probes[(int)id].min_cycles;
}

void debug_prof_dump()
{
    printf("--- Profiling dump (%u categories) ---\n", (uint32_t)_PROF_COUNT_);
    for(int i = 0; i < _PROF_COUNT_; i++){
        printf("Statistics for : %s\n",probes_names[i]);

        uint64_t average = probes[i].count > 0 ? probes[i].total_cycles / probes[i].count : 0;

        printf("Min : %llu, Average : %llu, Max : %llu\n", probes[i].min_cycles, 
            average, probes[i].max_cycles);
        printf("-------------------\n");
    }
}
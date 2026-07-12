#include "interrupts/irq.h"

// 32 bytes : 1 bit = one irq
uint8_t used_vectors[32] = {0};

uint8_t alloc_irq_vector()
{
    uint8_t next_msi_vector = 0x41;
    while(next_msi_vector <= 0xEF){
        if((used_vectors[next_msi_vector / 8] & (1 << (next_msi_vector % 8))) == 0){
            used_vectors[(next_msi_vector / 8)] |= (1 << (next_msi_vector % 8));
            return next_msi_vector;
        }
        next_msi_vector++;
    }
    return 0;
}
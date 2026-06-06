#include "memory/pool.h"

#include "interrupts/idt.h"

#include "stdio.h"

void pool_init(pool_t *pool, void *backing_memory, size_t total_bytes, size_t slot_size){

    if(slot_size < sizeof(void*)){
        KLOG_ERROR("MEM POOL", "Couldn't initialize pool : Invalid slot size : %u", slot_size);
        return;
    }

    pool->capacity = total_bytes / slot_size;
    pool->slot_size = slot_size;

    for(int i = 0; i < (int)pool->capacity - 1; i++){
        void* addr = backing_memory + i * slot_size;
        void* addr1 = backing_memory + (i + 1) * slot_size;
        *((void**)addr) = addr1;
    }
    void* last_addr = backing_memory + (pool->capacity - 1) * slot_size;
    *((void**)last_addr) = NULL;

    pool->free_head = backing_memory;
    pool->free_count = pool->capacity;
}

void* pool_alloc(pool_t* pool){
    uintptr_t int_state = disable_interrupts();

    if(pool->free_head == NULL){
        KLOG_WARN("MEM POOL", "Couldn't allocate in pool, as it is empty. Always call pool_init before pool_alloc !");
        restore_interrupts(int_state);
        return NULL;
    }

    void* result = pool->free_head;
    pool->free_head = *((void**)result);

    pool->free_count--;

    restore_interrupts(int_state);

    return result;
}

void pool_free(pool_t* pool, void* ptr){

    if(ptr == NULL){
        return;
    }

    uintptr_t int_state = disable_interrupts();

    *((void**)ptr) = pool->free_head;

    pool->free_head = ptr;

    pool->free_count++;

    restore_interrupts(int_state);
}
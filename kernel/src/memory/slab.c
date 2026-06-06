#include "memory/slab.h"

#include "stdio.h"
#include "string.h"

#include "memory/heap.h"
#include "memory/paging_frames.h"

#include "interrupts/idt.h"

// Inserts s at head
static void slab_list_push(slab_t** head, slab_t* s){
    s->prev = NULL;
    s->next = *head;
    if (*head != NULL)
        (*head)->prev = s;
    *head = s;
}

// Removes s from list at head
static void slab_list_remove(slab_t** head, slab_t* s){
    if(s->prev != NULL)
    {
        s->prev->next = s->next;
    }
    if(s->prev == NULL){
        *head = s->next;
    }
    if(s->next != NULL){
        s->next->prev = s->prev;
    }
    s->prev = s->next = NULL;
}

// Allocates and initializes a new slab for this cache
// Returns NULL if malloc_aligned fails
static slab_t* slab_create(slab_cache_t* cache){
    void* page = kmalloc_aligned(FRAME_SIZE, FRAME_SIZE);

    if(page == NULL){
        return NULL;
    }

    slab_t* s = (slab_t*)page;
    s->cache = cache;
    s->obj_count = cache->objs_per_slab;
    s->free_count = s->obj_count;
    s->prev = s->next = NULL;
    uint8_t* first_obj = (uint8_t*)page + cache->slab_header_size;
    for(int i = 0; (uint32_t)i < s->obj_count - 1; i++){
        void* addr = first_obj + i * cache->slot_size;
        void* addr1 = first_obj + (i + 1) * cache->slot_size;
        *((void**)addr) = addr1;
    }
    void* last_addr = first_obj + (s->obj_count - 1) * cache->slot_size;
    *((void**)last_addr) = NULL;

    s->free_head = first_obj;
    return s;
}

slab_cache_t* slab_cache_create(const char* name, size_t obj_size){

    slab_cache_t* cache = kmalloc(sizeof(slab_cache_t));

    strncpy(cache->name, name, 31); 

    cache->name[31] = '\0';

    cache->obj_size = obj_size;

    cache->slot_size = ALIGN_UP(obj_size, 8);

    cache->slab_header_size = ALIGN_UP(sizeof(slab_t), cache->slot_size);

    cache->objs_per_slab = (FRAME_SIZE - cache->slab_header_size) / cache->slot_size;

    if(cache->objs_per_slab == 0){
        KLOG_ERROR("MEM SLAB", "Couldn't create slab cache : Wrong object size");
        kfree(cache);
        return NULL;
    }

    cache->partial = cache->full = cache->empty = NULL;

    return cache;
}

void* slab_cache_alloc(slab_cache_t* cache){
    uintptr_t int_state = disable_interrupts();

    slab_t* s;

    if(cache->partial != NULL){
        s = cache->partial;
    }
    else if(cache->empty != NULL){
        s = cache->empty;
        slab_list_remove(&cache->empty, s);
        slab_list_push(&cache->partial, s);
    }
    else{
        s = slab_create(cache);
        if(s == NULL){
            restore_interrupts(int_state);
            return NULL;
        }
        else{
            slab_list_push(&cache->partial, s);
        }
    }

    void* result = s->free_head;

    s->free_head = *((void**)result);

    s->free_count--;

    if(s->free_count == 0)
    {
        slab_list_remove(&cache->partial, s);
        slab_list_push(&cache->full, s);
    }

    restore_interrupts(int_state);

    return result;
}

void slab_cache_free(void* ptr){

    if(ptr == NULL){
        return;
    }

    uintptr_t int_state = disable_interrupts();

    slab_t* s = (slab_t*)((uintptr_t)ptr & ~((uintptr_t)(FRAME_SIZE - 1)));

    slab_cache_t* cache = s->cache;

    bool was_full = s->free_count == 0;
    
    *((void**)ptr) = s->free_head;
    s->free_head = ptr;
    s->free_count++;

    if(was_full){
        slab_list_remove(&cache->full, s);
        slab_list_push(&cache->partial, s);
    }
    else if(s->free_count == s->obj_count){
        slab_list_remove(&cache->partial, s);
        slab_list_push(&cache->empty, s);
    }

    restore_interrupts(int_state);
}
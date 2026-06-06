#include "profiling/selftest/selftest.h"

#include "memory/slab.h"

//Self test slab
selftest_result_t selftest_slab(){
    slab_cache_t* cache = slab_cache_create("selftest_slab", 32);
    uint32_t* ptr;
    for(int i = 0; i < 10; i++){
        ptr = (uint32_t*)slab_cache_alloc(cache);
        if(ptr == NULL)
            return TEST_FAIL;
        slab_cache_free(ptr);
    }
    uint32_t* ptr2 = (uint32_t*)slab_cache_alloc(cache);
    slab_cache_free(ptr2);
    uint32_t* ptr3 = (uint32_t*)slab_cache_alloc(cache);
    if(ptr2 != ptr3)
        return TEST_FAIL;
    uint32_t* ptr4 = slab_cache_alloc(cache);
    *ptr4 = 0xABBA;
    uint32_t* ptr5 = slab_cache_alloc(cache);
    if(*ptr4 != 0xABBA){
        slab_cache_free(ptr4);
        slab_cache_free(ptr5);
        return TEST_FAIL;
    }
    
    slab_cache_free(ptr4);
    slab_cache_free(ptr5);

    return TEST_PASS;
}

SELFTEST_REGISTER(slab_test, selftest_slab)
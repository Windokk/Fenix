#include "profiling/selftest/selftest.h"

#include "memory/heap.h"

#include "stdio.h"

//Self test heap
selftest_result_t selftest_heap(){
    uint32_t* ptr = kmalloc(16);
    *ptr = 125;
    kfree(ptr);
    void* ptr1 = kmalloc(16);
    if(ptr1 != ptr)
        return TEST_FAIL;
    kmalloc(0);
    void* ptr2 = kmalloc_aligned(563, 16);
    if((uintptr_t)ptr2 % 16 != 0){
        return TEST_FAIL;
    }
    kfree(ptr1);
    kfree_aligned(ptr2);
    return TEST_PASS;
}

SELFTEST_REGISTER(heap_test, selftest_heap)
#include "profiling/selftest/selftest.h"

#include "drivers/serial.h"
#include "stdio.h"

extern const selftest_t __selftests_start[];
extern const selftest_t __selftests_end[];

void selftest_run_all(void) {
    for (const selftest_t *t = __selftests_start; t < __selftests_end; t++) {
        selftest_result_t r = t->fn();
        if(r == TEST_PASS){
            KLOG_INFO("PASS", "%s", t->name);
        }
        else if (r == TEST_FAIL){
            KLOG_ERROR("FAIL", "%s", t->name);
        }
        else{
            KLOG_WARN("SKIP", "%s", t->name);
        }

    }
}
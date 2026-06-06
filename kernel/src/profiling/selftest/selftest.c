#include "profiling/selftest/selftest.h"

#include "drivers/serial.h"
#include "stdio.h"

extern const selftest_t __selftests_start[];
extern const selftest_t __selftests_end[];

void selftest_run_all(void) {
    for (const selftest_t *t = __selftests_start; t < __selftests_end; t++) {
        selftest_result_t r = t->fn();
        if      (r == TEST_PASS) serial_print("[PASS] ");
        else if (r == TEST_FAIL) serial_print("[FAIL] ");
        else                     serial_print("[SKIP] ");
        serial_print(t->name);
        serial_print("\n");
    }
}
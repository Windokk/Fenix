#pragma once

typedef enum { TEST_PASS, TEST_FAIL, TEST_SKIP } selftest_result_t;

typedef struct {
    const char *name;
    selftest_result_t (*fn)(void);
} selftest_t;

#define SELFTEST_REGISTER(test_name, test_fn)                    \
    static const selftest_t _test_##test_name                    \
        __attribute__((section(".selftests"), used)) = {         \
            .name = #test_name,                                  \
            .fn   = test_fn,                                     \
        };

extern void selftest_run_all();
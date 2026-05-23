#ifndef __COMMON_H
#define __COMMON_H

#if __STDC_VERSION__ < 202311L
typedef _Bool bool;
#endif

// Define fixed-width unsigned types
typedef unsigned char      uint8_t;   // 8-bit unsigned
typedef unsigned short     uint16_t;  // 16-bit unsigned
typedef unsigned int       uint32_t;  // 32-bit unsigned
typedef unsigned long long uint64_t;  // 64-bit unsigned

// Define fixed-width signed types
typedef signed char        int8_t;    // 8-bit signed
typedef signed short       int16_t;   // 16-bit signed
typedef signed int         int32_t;   // 32-bit signed
typedef signed long long   int64_t;   // 64-bit signed

// Define max-width types
typedef unsigned long long uintmax_t; // Maximum-width unsigned type
typedef long long          intmax_t;  // Maximum-width signed type

// Define pointer-sized types (adapt to architecture)
typedef unsigned long      uintptr_t; // Unsigned integer of pointer size
typedef long               intptr_t;  // Signed integer of pointer size

typedef unsigned long size_t;

#define true 1
#define false 0

#define NULL ((void *)0)

#define UINT64_MAX		(__UINT64_C(18446744073709551615))

#define CHAR_BIT	8

#define ARR_SIZE(arr)		sizeof(arr)/sizeof(arr[0])

#define abs(num) (num >= 0 ? num : num*-1)

#define PI 3.141592653589793f


#define SQRT_SINGLE(num) ({              \
    float result;                        \
    __asm__(                             \
        "sqrtss %1, %0"                  \
        : "=x" (result)                   \
        : "x" (num)                       \
    );                                    \
    result;                              \
})

#define SQRT_DOUBLE(num) ({             \
    double result;                      \
    __asm__(                            \
        "sqrtsd %1, %0"                 \
        : "=x" (result)                  \
        : "x" (num)                      \
    );                                   \
    result;                             \
})

#define sqrt(num) (                 \
    sizeof(num) == sizeof(float) ?    \
        SQRT_SINGLE(num) :            \
        (sizeof(num) == sizeof(double) ? \
            SQRT_DOUBLE(num) : 0.0) \
)


#define COS(x) ({ \
    float result; \
    __asm__ ( \
        "fld %1;"                 /* Load x into FPU stack */ \
        "fcos;"                   /* Compute cosine */ \
        "fstp %0;"                /* Store result and pop from FPU stack */ \
        : "=m"(result)            /* Output: result stored in memory */ \
        : "t"(x)                  /* Input: x in the FPU stack (t register) */ \
    ); \
    result; \
})



#endif 
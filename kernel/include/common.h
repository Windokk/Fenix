#pragma once

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

// Define pointer-sized types
typedef unsigned long      uintptr_t; // Unsigned integer of pointer size
typedef long               intptr_t;  // Signed integer of pointer size

typedef unsigned long size_t;

# undef __INT64_C
# undef __UINT64_C
# if __WORDSIZE == 64
#  define __INT64_C(c)	c ## L
#  define __UINT64_C(c)	c ## UL
# else
#  define __INT64_C(c)	c ## LL
#  define __UINT64_C(c)	c ## ULL
# endif

/* Minimum of signed integral types.  */
# define INT8_MIN		(-128)
# define INT16_MIN		(-32767-1)
# define INT32_MIN		(-2147483647-1)
# define INT64_MIN		(-__INT64_C(9223372036854775807)-1)
/* Maximum of signed integral types.  */
# define INT8_MAX		(127)
# define INT16_MAX		(32767)
# define INT32_MAX		(2147483647)
# define INT64_MAX		(__INT64_C(9223372036854775807))

/* Maximum of unsigned integral types.  */
# define UINT8_MAX		(255)
# define UINT16_MAX		(65535)
# define UINT32_MAX		(4294967295U)
# define UINT64_MAX		(__UINT64_C(18446744073709551615))

#define true 1
#define false 0

#define NULL ((void *)0)

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

extern void HALT_AND_CATCH_FIRE(const char *fmt, ...);

#define KLOG_INFO(tag, fmt, ...)  do { \
        fb_set_color(80, 210, 90); \
        printf("[" tag " INFO] "); \  
        fb_set_color(255, 255, 255); \
        printf(fmt "\n", ##__VA_ARGS__); \
    } while(0)

#define KLOG_WARN(tag, fmt, ...)  do { \  
        fb_set_color(255, 180, 0); \
        printf("[" tag " WARN] "); \  
        fb_set_color(255, 255, 255); \
        printf(fmt "\n", ##__VA_ARGS__); \
    } while(0)

#define KLOG_ERROR(tag, fmt, ...) do { \
        fb_set_color(255, 60, 60); \
        printf("[" tag " ERROR] "); \  
        fb_set_color(255, 255, 255); \
        printf(fmt "\n", ##__VA_ARGS__); \
    } while(0)

#define KLOG_FATAL(tag, fmt, ...)  do { \
        fb_set_color(255, 0, 0); \
        printf("[" tag " FATAL] "); \  
        fb_set_color(255, 255, 255); \
        printf(fmt "\n", ##__VA_ARGS__); \
        HALT_AND_CATCH_FIRE("[" tag " FATAL] " fmt, ##__VA_ARGS__); \
    } while(0)

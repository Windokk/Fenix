#ifndef __COMMON_H
#define __COMMON_H

typedef _Bool bool;

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


#endif
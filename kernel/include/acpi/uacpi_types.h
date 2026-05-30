#pragma once

/*
 * Fenix-specific uACPI type overrides (UACPI_OVERRIDE_TYPES).
 * Maps uACPI primitive types onto our freestanding typedefs from common.h,
 * avoiding conflicts with <stdint.h>/<stdbool.h> in freestanding builds.
 *
 * NOTE: compiler.h is still pulled in by the normal include chain; we do NOT
 * override it.  Only types go here.
 */

#include "common.h"

/* Integer types */
typedef uint8_t  uacpi_u8;
typedef uint16_t uacpi_u16;
typedef uint32_t uacpi_u32;
typedef uint64_t uacpi_u64;

typedef int8_t   uacpi_i8;
typedef int16_t  uacpi_i16;
typedef int32_t  uacpi_i32;
typedef int64_t  uacpi_i64;

/* Boolean */
typedef bool     uacpi_bool;
#define UACPI_TRUE   true
#define UACPI_FALSE  false
#define UACPI_NULL   NULL

/* Pointer-width types */
typedef uintptr_t     uacpi_uintptr;
typedef uacpi_uintptr uacpi_virt_addr;
typedef unsigned long uacpi_size;

/* va_list — use GCC builtins (always available in freestanding GCC) */
typedef __builtin_va_list uacpi_va_list;
#define uacpi_va_start __builtin_va_start
#define uacpi_va_end   __builtin_va_end
#define uacpi_va_arg   __builtin_va_arg

typedef char uacpi_char;

#define uacpi_offsetof __builtin_offsetof

/* 64-bit printf format macros */
#define UACPI_PRIu64  "llu"
#define UACPI_PRIx64  "llx"
#define UACPI_PRIX64  "llX"
#define UACPI_FMT64(val)  ((unsigned long long)(val))

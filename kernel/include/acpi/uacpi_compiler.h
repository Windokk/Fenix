#pragma once

/*
 * Fenix-specific uACPI compiler overrides (UACPI_OVERRIDE_COMPILER).
 * Uses GCC/Clang attributes instead of MSVC __declspec.
 */

#define UACPI_STATIC_ASSERT           _Static_assert
#define UACPI_ALIGN(x)                __attribute__((aligned(x)))
#define UACPI_ALWAYS_INLINE           inline __attribute__((always_inline))
#define UACPI_PACKED(decl)            decl __attribute__((packed));

#define uacpi_unlikely(expr)          __builtin_expect(!!(expr), 0)
#define uacpi_likely(expr)            __builtin_expect(!!(expr), 1)

#define UACPI_PRINTF_DECL(fmt, args)  __attribute__((format(printf, fmt, args)))

#define UACPI_NO_UNUSED_PARAMETER_WARNINGS_BEGIN \
    _Pragma("GCC diagnostic push") \
    _Pragma("GCC diagnostic ignored \"-Wunused-parameter\"")
#define UACPI_NO_UNUSED_PARAMETER_WARNINGS_END \
    _Pragma("GCC diagnostic pop")

#define UACPI_FALLTHROUGH   __attribute__((fallthrough))

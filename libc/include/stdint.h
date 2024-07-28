#pragma once

#ifndef __STDINT_H
#define __STDINT_H

typedef signed char __int8_t;
typedef unsigned char __uint8_t;
typedef signed short int __int16_t;
typedef unsigned short int __uint16_t;
typedef signed int __int32_t;
typedef unsigned int __uint32_t;
#if __WORDSIZE == 64
typedef signed long int __int64_t;
typedef unsigned long int __uint64_t;

#else
__extension__ typedef signed long long int __int64_t;
__extension__ typedef unsigned long long int __uint64_t;

#endif



typedef __int8_t int8_t;
typedef __int16_t int16_t;
typedef __int32_t int32_t;

typedef __uint8_t uint8_t;
typedef __uint16_t uint16_t;
typedef __uint32_t uint32_t;

typedef __int64_t int64_t;
typedef __uint64_t uint64_t;

typedef long long ssize_t;

typedef unsigned long uintptr_t;

#endif
#ifndef __CDEFS_H
#define __CDEFS_H

#define __fenix_libc

#ifdef __STRICT_ANSI__
	#define ASM_VOLATILE(n) __asm__ __volatile__ (n)
	#define ASM(n) __asm__(n)
#else
	#define ASM_VOLATILE(n) asm volatile (n)
	#define ASM(n) asm(n)
#endif

#endif
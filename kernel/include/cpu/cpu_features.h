#pragma once

#include "common.h"

// Vendor strings from CPUs.
#define CPUID_VENDOR_AMD           "AuthenticAMD"
#define CPUID_VENDOR_AMD_OLD       "AMDisbetter!" // Early engineering samples of AMD K5 processor
#define CPUID_VENDOR_INTEL         "GenuineIntel"
#define CPUID_VENDOR_VIA           "VIA VIA VIA "
#define CPUID_VENDOR_TRANSMETA     "GenuineTMx86"
#define CPUID_VENDOR_TRANSMETA_OLD "TransmetaCPU"
#define CPUID_VENDOR_CYRIX         "CyrixInstead"
#define CPUID_VENDOR_CENTAUR       "CentaurHauls"
#define CPUID_VENDOR_NEXGEN        "NexGenDriven"
#define CPUID_VENDOR_UMC           "UMC UMC UMC "
#define CPUID_VENDOR_SIS           "SiS SiS SiS "
#define CPUID_VENDOR_NSC           "Geode by NSC"
#define CPUID_VENDOR_RISE          "RiseRiseRise"
#define CPUID_VENDOR_VORTEX        "Vortex86 SoC"
#define CPUID_VENDOR_AO486         "MiSTer AO486"
#define CPUID_VENDOR_AO486_OLD     "GenuineAO486"
#define CPUID_VENDOR_ZHAOXIN       "  Shanghai  "
#define CPUID_VENDOR_HYGON         "HygonGenuine"
#define CPUID_VENDOR_ELBRUS        "E2K MACHINE "
 
// Vendor strings from hypervisors.
#define CPUID_VENDOR_QEMU          "TCGTCGTCGTCG"
#define CPUID_VENDOR_KVM           " KVMKVMKVM  "
#define CPUID_VENDOR_VMWARE        "VMwareVMware"
#define CPUID_VENDOR_VIRTUALBOX    "VBoxVBoxVBox"
#define CPUID_VENDOR_XEN           "XenVMMXenVMM"
#define CPUID_VENDOR_HYPERV        "Microsoft Hv"
#define CPUID_VENDOR_PARALLELS     " prl hyperv "
#define CPUID_VENDOR_PARALLELS_ALT " lrpepyh vr " // Sometimes Parallels incorrectly encodes "prl hyperv" as "lrpepyh vr" due to an endianness mismatch.
#define CPUID_VENDOR_BHYVE         "bhyve bhyve "
#define CPUID_VENDOR_QNX           " QNXQVMBSQG "

enum {
    CPUID_FEAT_EBX_BRAND_INDEX        = 0xFF << 0,
    CPUID_FEAT_EBX_CLFLUSH_LINE_SIZE  = 0xFF << 8,
    CPUID_FEAT_EBX_APIC_ID_SPACE      = 0xFF << 16,
    CPUID_FEAT_EBX_INITIAL_APIC_ID    = 0xFF << 24,
    
    CPUID_FEAT_ECX_SSE3               = 1 << 0,
    CPUID_FEAT_ECX_PCLMUL             = 1 << 1,
    CPUID_FEAT_ECX_DTES64             = 1 << 2,
    CPUID_FEAT_ECX_MONITOR            = 1 << 3,
    CPUID_FEAT_ECX_DS_CPL             = 1 << 4,
    CPUID_FEAT_ECX_VMX                = 1 << 5,
    CPUID_FEAT_ECX_SMX                = 1 << 6,
    CPUID_FEAT_ECX_EST                = 1 << 7,
    CPUID_FEAT_ECX_TM2                = 1 << 8,
    CPUID_FEAT_ECX_SSSE3              = 1 << 9,
    CPUID_FEAT_ECX_CID                = 1 << 10,
    CPUID_FEAT_ECX_SDBG               = 1 << 11,
    CPUID_FEAT_ECX_FMA                = 1 << 12,
    CPUID_FEAT_ECX_CX16               = 1 << 13,
    CPUID_FEAT_ECX_XTPR               = 1 << 14,
    CPUID_FEAT_ECX_PDCM               = 1 << 15,
    CPUID_FEAT_ECX_PCID               = 1 << 17,
    CPUID_FEAT_ECX_DCA                = 1 << 18,
    CPUID_FEAT_ECX_SSE4_1             = 1 << 19,
    CPUID_FEAT_ECX_SSE4_2             = 1 << 20,
    CPUID_FEAT_ECX_X2APIC             = 1 << 21,
    CPUID_FEAT_ECX_MOVBE              = 1 << 22,
    CPUID_FEAT_ECX_POPCNT             = 1 << 23,
    CPUID_FEAT_ECX_TSC                = 1 << 24,
    CPUID_FEAT_ECX_AES                = 1 << 25,
    CPUID_FEAT_ECX_XSAVE              = 1 << 26,
    CPUID_FEAT_ECX_OSXSAVE            = 1 << 27,
    CPUID_FEAT_ECX_AVX                = 1 << 28,
    CPUID_FEAT_ECX_F16C               = 1 << 29,
    CPUID_FEAT_ECX_RDRAND             = 1 << 30,
    CPUID_FEAT_ECX_HYPERVISOR         = 1u << 31,

    CPUID_FEAT_EDX_FPU                = 1 << 0,
    CPUID_FEAT_EDX_VME                = 1 << 1,
    CPUID_FEAT_EDX_DE                 = 1 << 2,
    CPUID_FEAT_EDX_PSE                = 1 << 3,
    CPUID_FEAT_EDX_TSC                = 1 << 4,
    CPUID_FEAT_EDX_MSR                = 1 << 5,
    CPUID_FEAT_EDX_PAE                = 1 << 6,
    CPUID_FEAT_EDX_MCE                = 1 << 7,
    CPUID_FEAT_EDX_CX8                = 1 << 8,
    CPUID_FEAT_EDX_APIC               = 1 << 9,
    CPUID_FEAT_EDX_SEP                = 1 << 11,
    CPUID_FEAT_EDX_MTRR               = 1 << 12,
    CPUID_FEAT_EDX_PGE                = 1 << 13,
    CPUID_FEAT_EDX_MCA                = 1 << 14,
    CPUID_FEAT_EDX_CMOV               = 1 << 15,
    CPUID_FEAT_EDX_PAT                = 1 << 16,
    CPUID_FEAT_EDX_PSE36              = 1 << 17,
    CPUID_FEAT_EDX_PSN                = 1 << 18,
    CPUID_FEAT_EDX_CLFLUSH            = 1 << 19,
    CPUID_FEAT_EDX_DS                 = 1 << 21,
    CPUID_FEAT_EDX_ACPI               = 1 << 22,
    CPUID_FEAT_EDX_MMX                = 1 << 23,
    CPUID_FEAT_EDX_FXSR               = 1 << 24,
    CPUID_FEAT_EDX_SSE                = 1 << 25,
    CPUID_FEAT_EDX_SSE2               = 1 << 26,
    CPUID_FEAT_EDX_SS                 = 1 << 27,
    CPUID_FEAT_EDX_HTT                = 1 << 28,
    CPUID_FEAT_EDX_TM                 = 1 << 29,
    CPUID_FEAT_EDX_IA64               = 1 << 30,
    CPUID_FEAT_EDX_PBE                = 1u << 31,

    CPUID_FEAT_EDX_INV_TSC            = 1 << 8,
    CPUID_FEAT_EDX_RDTSCP             = 1 << 27
};

/*
 * Copyright (C) 2007-2026 Free Software Foundation, Inc.
 *
 * This file is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 3, or (at your option) any
 * later version.
 *
 * This file is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * Under Section 7 of GPL version 3, you are granted additional
 * permissions described in the GCC Runtime Library Exception, version
 * 3.1, as published by the Free Software Foundation.
 *
 * You should have received a copy of the GNU General Public License and
 * a copy of the GCC Runtime Library Exception along with this program;
 * see the files COPYING3 and COPYING.RUNTIME respectively.  If not, see
 * <http://www.gnu.org/licenses/>.
 */

#define __cpuid(level, a, b, c, d)					\
  do {									\
    if (__builtin_constant_p (level) && (level) != 1)			\
      __asm__ __volatile__ ("cpuid\n\t"					\
			    : "=a" (a), "=b" (b), "=c" (c), "=d" (d)	\
			    : "0" (level));				\
    else								\
      __asm__ __volatile__ ("cpuid\n\t"					\
			    : "=a" (a), "=b" (b), "=c" (c), "=d" (d)	\
			    : "0" (level), "1" (0), "2" (0));		\
  } while (0)


static inline unsigned int
__get_cpuid_max (unsigned int __ext, unsigned int *__sig)
{
  unsigned int __eax, __ebx, __ecx, __edx;

  /* Host supports cpuid.  Return highest supported cpuid input value.  */
  __cpuid (__ext, __eax, __ebx, __ecx, __edx);

  if (__sig)
    *__sig = __ebx;

  return __eax;
}

static inline int __get_cpuid (unsigned int __leaf,
	     unsigned int *__eax, unsigned int *__ebx,
	     unsigned int *__ecx, unsigned int *__edx)
{
  unsigned int __ext = __leaf & 0x80000000;
  unsigned int __maxlevel = __get_cpuid_max (__ext, 0);

  if (__maxlevel == 0 || __maxlevel < __leaf)
    return 0;

  __cpuid (__leaf, *__eax, *__ebx, *__ecx, *__edx);
  return 1;
}

typedef struct cpu_features_t {
    /* CPUID.01H:ECX */
    bool sse3;
    bool pclmul;
    bool dtes64;
    bool monitor;
    bool ds_cpl;
    bool vmx;
    bool smx;
    bool est;
    bool tm2;
    bool ssse3;
    bool cid;
    bool sdbg;
    bool fma;
    bool cx16;
    bool xtpr;
    bool pdcm;
    bool pcid;
    bool dca;
    bool sse4_1;
    bool sse4_2;
    bool x2apic;
    bool movbe;
    bool popcnt;
    bool tsc_deadline;
    bool aes;
    bool xsave;
    bool osxsave;
    bool avx;
    bool f16c;
    bool rdrand;
    bool hypervisor;

    /* CPUID.01H:EDX */
    bool fpu;
    bool vme;
    bool de;
    bool pse;
    bool tsc;
    bool msr;
    bool pae;
    bool mce;
    bool cx8;
    bool apic;
    bool sep;
    bool mtrr;
    bool pge;
    bool mca;
    bool cmov;
    bool pat;
    bool pse36;
    bool psn;
    bool clflush;
    bool ds;
    bool acpi;
    bool mmx;
    bool fxsr;
    bool sse;
    bool sse2;
    bool ss;
    bool htt;
    bool tm;
    bool ia64;
    bool pbe;

    bool invariant_tsc;
    bool rdtscp;

} cpu_features_t;

extern void cpu_get_features();

extern cpu_features_t cpu_features;
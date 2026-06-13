#include "cpu/cpu_features.h"

#include "string.h"

cpu_features_t cpu_features;

void cpu_get_features()
{
    unsigned eax, ebx, ecx, edx;

    if (!__get_cpuid(1, &eax, &ebx, &ecx, &edx))
        return;

    /* ECX */
    cpu_features.sse3        = ecx & CPUID_FEAT_ECX_SSE3;
    cpu_features.pclmul      = ecx & CPUID_FEAT_ECX_PCLMUL;
    cpu_features.dtes64      = ecx & CPUID_FEAT_ECX_DTES64;
    cpu_features.monitor     = ecx & CPUID_FEAT_ECX_MONITOR;
    cpu_features.ds_cpl      = ecx & CPUID_FEAT_ECX_DS_CPL;
    cpu_features.vmx         = ecx & CPUID_FEAT_ECX_VMX;
    cpu_features.smx         = ecx & CPUID_FEAT_ECX_SMX;
    cpu_features.est         = ecx & CPUID_FEAT_ECX_EST;
    cpu_features.tm2         = ecx & CPUID_FEAT_ECX_TM2;
    cpu_features.ssse3       = ecx & CPUID_FEAT_ECX_SSSE3;
    cpu_features.cid         = ecx & CPUID_FEAT_ECX_CID;
    cpu_features.sdbg        = ecx & CPUID_FEAT_ECX_SDBG;
    cpu_features.fma         = ecx & CPUID_FEAT_ECX_FMA;
    cpu_features.cx16        = ecx & CPUID_FEAT_ECX_CX16;
    cpu_features.xtpr        = ecx & CPUID_FEAT_ECX_XTPR;
    cpu_features.pdcm        = ecx & CPUID_FEAT_ECX_PDCM;
    cpu_features.pcid        = ecx & CPUID_FEAT_ECX_PCID;
    cpu_features.dca         = ecx & CPUID_FEAT_ECX_DCA;
    cpu_features.sse4_1      = ecx & CPUID_FEAT_ECX_SSE4_1;
    cpu_features.sse4_2      = ecx & CPUID_FEAT_ECX_SSE4_2;
    cpu_features.x2apic      = ecx & CPUID_FEAT_ECX_X2APIC;
    cpu_features.movbe       = ecx & CPUID_FEAT_ECX_MOVBE;
    cpu_features.popcnt      = ecx & CPUID_FEAT_ECX_POPCNT;
    cpu_features.tsc_deadline= ecx & CPUID_FEAT_ECX_TSC;
    cpu_features.aes         = ecx & CPUID_FEAT_ECX_AES;
    cpu_features.xsave       = ecx & CPUID_FEAT_ECX_XSAVE;
    cpu_features.osxsave     = ecx & CPUID_FEAT_ECX_OSXSAVE;
    cpu_features.avx         = ecx & CPUID_FEAT_ECX_AVX;
    cpu_features.f16c        = ecx & CPUID_FEAT_ECX_F16C;
    cpu_features.rdrand      = ecx & CPUID_FEAT_ECX_RDRAND;
    cpu_features.hypervisor  = ecx & CPUID_FEAT_ECX_HYPERVISOR;

    /* EDX */
    cpu_features.fpu         = edx & CPUID_FEAT_EDX_FPU;
    cpu_features.vme         = edx & CPUID_FEAT_EDX_VME;
    cpu_features.de          = edx & CPUID_FEAT_EDX_DE;
    cpu_features.pse         = edx & CPUID_FEAT_EDX_PSE;
    cpu_features.tsc         = edx & CPUID_FEAT_EDX_TSC;
    cpu_features.msr         = edx & CPUID_FEAT_EDX_MSR;
    cpu_features.pae         = edx & CPUID_FEAT_EDX_PAE;
    cpu_features.mce         = edx & CPUID_FEAT_EDX_MCE;
    cpu_features.cx8         = edx & CPUID_FEAT_EDX_CX8;
    cpu_features.apic        = edx & CPUID_FEAT_EDX_APIC;
    cpu_features.sep         = edx & CPUID_FEAT_EDX_SEP;
    cpu_features.mtrr        = edx & CPUID_FEAT_EDX_MTRR;
    cpu_features.pge         = edx & CPUID_FEAT_EDX_PGE;
    cpu_features.mca         = edx & CPUID_FEAT_EDX_MCA;
    cpu_features.cmov        = edx & CPUID_FEAT_EDX_CMOV;
    cpu_features.pat         = edx & CPUID_FEAT_EDX_PAT;
    cpu_features.pse36       = edx & CPUID_FEAT_EDX_PSE36;
    cpu_features.psn         = edx & CPUID_FEAT_EDX_PSN;
    cpu_features.clflush     = edx & CPUID_FEAT_EDX_CLFLUSH;
    cpu_features.ds          = edx & CPUID_FEAT_EDX_DS;
    cpu_features.acpi        = edx & CPUID_FEAT_EDX_ACPI;
    cpu_features.mmx         = edx & CPUID_FEAT_EDX_MMX;
    cpu_features.fxsr        = edx & CPUID_FEAT_EDX_FXSR;
    cpu_features.sse         = edx & CPUID_FEAT_EDX_SSE;
    cpu_features.sse2        = edx & CPUID_FEAT_EDX_SSE2;
    cpu_features.ss          = edx & CPUID_FEAT_EDX_SS;
    cpu_features.htt         = edx & CPUID_FEAT_EDX_HTT;
    cpu_features.tm          = edx & CPUID_FEAT_EDX_TM;
    cpu_features.ia64        = edx & CPUID_FEAT_EDX_IA64;
    cpu_features.pbe         = edx & CPUID_FEAT_EDX_PBE;

    if (!__get_cpuid(0x80000007, &eax, &ebx, &ecx, &edx))
        return;

    cpu_features.invariant_tsc = edx & (1 << 8);
    cpu_features.rdtscp = edx & (1 << 27);
}
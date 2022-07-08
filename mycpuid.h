#pragma once

/*
    P6Cache - Pentium II/III/Celeron L2 Cache management utility
    --wbcbz7 o9.o7.2o22
    
    p6cache.cpp, mycpuid.cpp/mycpuid.h and lowlevel.h are licensed under following terms:

    Copyright (c) 2022 Artem Vasilev - wbcbz7

    Permission is hereby granted, free of charge, to any person obtaining a copy
    of this software and associated documentation files (the "Software"), to deal
    in the Software without restriction, including without limitation the rights
    to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
    copies of the Software, and to permit persons to whom the Software is
    furnished to do so, subject to the following conditions:

    The above copyright notice and this permission notice shall be included in all
    copies or substantial portions of the Software.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
    IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
    FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
    AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
    LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
    OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
    SOFTWARE.

    l2_cache.c and all files from "coreboot" folder are derived from coreboot project
    and licensed under GNU GPL 2.0 terms
*/

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// feature masks
enum {
    CPUID_EXTFEATURE_SSE3      = (1 << 0), 
    CPUID_EXTFEATURE_PCLMUL    = (1 << 1),
    CPUID_EXTFEATURE_DTES64    = (1 << 2),
    CPUID_EXTFEATURE_MONITOR   = (1 << 3),  
    CPUID_EXTFEATURE_DS_CPL    = (1 << 4),  
    CPUID_EXTFEATURE_VMX       = (1 << 5),  
    CPUID_EXTFEATURE_SMX       = (1 << 6),  
    CPUID_EXTFEATURE_EST       = (1 << 7),  
    CPUID_EXTFEATURE_TM2       = (1 << 8),  
    CPUID_EXTFEATURE_SSSE3     = (1 << 9),  
    CPUID_EXTFEATURE_CID       = (1 << 10),
    CPUID_EXTFEATURE_FMA       = (1 << 12),
    CPUID_EXTFEATURE_CX16      = (1 << 13), 
    CPUID_EXTFEATURE_ETPRD     = (1 << 14), 
    CPUID_EXTFEATURE_PDCM      = (1 << 15), 
    CPUID_EXTFEATURE_DCA       = (1 << 18), 
    CPUID_EXTFEATURE_SSE4_1    = (1 << 19), 
    CPUID_EXTFEATURE_SSE4_2    = (1 << 20), 
    CPUID_EXTFEATURE_x2APIC    = (1 << 21), 
    CPUID_EXTFEATURE_MOVBE     = (1 << 22), 
    CPUID_EXTFEATURE_POPCNT    = (1 << 23), 
    CPUID_EXTFEATURE_AES       = (1 << 25), 
    CPUID_EXTFEATURE_XSAVE     = (1 << 26), 
    CPUID_EXTFEATURE_OSXSAVE   = (1 << 27), 
    CPUID_EXTFEATURE_AVX       = (1 << 28),
 
    CPUID_FEATURE_FPU          = (1 << 0),  
    CPUID_FEATURE_VME          = (1 << 1),  
    CPUID_FEATURE_DE           = (1 << 2),  
    CPUID_FEATURE_PSE          = (1 << 3),  
    CPUID_FEATURE_TSC          = (1 << 4),  
    CPUID_FEATURE_MSR          = (1 << 5),  
    CPUID_FEATURE_PAE          = (1 << 6),  
    CPUID_FEATURE_MCE          = (1 << 7),  
    CPUID_FEATURE_CX8          = (1 << 8),  
    CPUID_FEATURE_APIC         = (1 << 9),  
    CPUID_FEATURE_SEP          = (1 << 11), 
    CPUID_FEATURE_MTRR         = (1 << 12), 
    CPUID_FEATURE_PGE          = (1 << 13), 
    CPUID_FEATURE_MCA          = (1 << 14), 
    CPUID_FEATURE_CMOV         = (1 << 15), 
    CPUID_FEATURE_PAT          = (1 << 16), 
    CPUID_FEATURE_PSE36        = (1 << 17), 
    CPUID_FEATURE_PSN          = (1 << 18), 
    CPUID_FEATURE_CLF          = (1 << 19), 
    CPUID_FEATURE_DTES         = (1 << 21), 
    CPUID_FEATURE_ACPI         = (1 << 22), 
    CPUID_FEATURE_MMX          = (1 << 23), 
    CPUID_FEATURE_FXSR         = (1 << 24), 
    CPUID_FEATURE_SSE          = (1 << 25), 
    CPUID_FEATURE_SSE2         = (1 << 26), 
    CPUID_FEATURE_SS           = (1 << 27), 
    CPUID_FEATURE_HTT          = (1 << 28), 
    CPUID_FEATURE_TM1          = (1 << 29), 
    CPUID_FEATURE_IA64         = (1 << 30),
    CPUID_FEATURE_PBE          = (1 << 31)
};

// cpuid structure
typedef struct {
    unsigned long   supported;
    unsigned long   highestLeaf;
    
    unsigned long   family;
    unsigned long   model;
    unsigned long   stepping;
    unsigned long   extfamily;
    unsigned long   extmodel;
    
    unsigned long   flags;
    unsigned long   extflags;
    
    // CPU vendor string, returned by leaf 0
    char            str[13];
} cpuid_t;

// execute cpuid for givel leaf
uint32_t cpuid(uint32_t leaf,  uint32_t *_eax, uint32_t *_ebx, uint32_t *_ecx, uint32_t *_edx);

// get CPUID info
uint32_t cpuidget(cpuid_t *p);


#ifdef __cplusplus
}
#endif


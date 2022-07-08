/* SPDX-License-Identifier: GPL-2.0-only */

#ifndef CPU_X86_CACHE
#define CPU_X86_CACHE

#include "cr.h"

#define CR0_CacheDisable	(CR0_CD)
#define CR0_NoWriteThrough	(CR0_NW)

#define CPUID_FEATURE_CLFLUSH_BIT 19

void wbinvd();
#pragma aux wbinvd = "wbinvd"

void invd();
#pragma aux invd = "invd"

void enable_cache(void);
void disable_cache(void);

#endif /* CPU_X86_CACHE */

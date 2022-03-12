/* SPDX-License-Identifier: GPL-2.0-only */

#ifndef __ASM_CACHE_H
#define __ASM_CACHE_H

void v8_invalidate_icache_all(void);
void v8_flush_dcache_all(void);
void v8_invalidate_dcache_all(void);
void v8_flush_dcache_range(unsigned long start, unsigned long end);
void v8_inv_dcache_range(unsigned long start, unsigned long end);

#ifdef CONFIG_CPU_32v7M
void icache_invalidate(void);
#else
static inline void icache_invalidate(void)
{
#if __LINUX_ARM_ARCH__ <= 7
	asm volatile("mcr p15, 0, %0, c7, c5, 0" : : "r" (0));
#else
	v8_invalidate_icache_all();
#endif
}
#endif

int arm_set_cache_functions(void);

void arm_early_mmu_cache_flush(void);
void arm_early_mmu_cache_invalidate(void);

#define sync_caches_for_execution sync_caches_for_execution
void sync_caches_for_execution(void);

#include <asm-generic/cache.h>

#endif

/* SPDX-License-Identifier: GPL-2.0-or-later */

#ifndef __ASM_ARM_CACHE_ARMV7M_H_
#define __ASM_ARM_CACHE_ARMV7M_H_

void v7m_mmu_cache_on(void);
void v7m_mmu_cache_off(void);
void v7m_mmu_cache_flush(void);
void v7m_mmu_cache_invalidate(void);

void v7m_dma_inv_range(unsigned long start, unsigned long stop);
void v7m_dma_clean_range(unsigned long start, unsigned long stop);
void v7m_dma_flush_range(unsigned long start, unsigned long stop);

#endif

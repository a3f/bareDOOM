// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) 2017, STMicroelectronics - All Rights Reserved
 * Author(s): Vikas Manocha, <vikas.manocha@st.com> for STMicroelectronics.
 */

#include <common.h>
#include <errno.h>
#include <asm/cache-armv7m.h>
#include <asm/armv7m.h>
#include <asm/system.h>
#include <asm/io.h>
#include <asm/cache.h>
#include <linux/bitops.h>

/* Cache maintenance operation registers */

#define V7M_CACHE_REG_ICIALLU		((u32 *)(V7M_CACHE_MAINT_BASE + 0x00))
#define INVAL_ICACHE_POU		0
#define V7M_CACHE_REG_ICIMVALU		((u32 *)(V7M_CACHE_MAINT_BASE + 0x08))
#define V7M_CACHE_REG_DCIMVAC		((u32 *)(V7M_CACHE_MAINT_BASE + 0x0C))
#define V7M_CACHE_REG_DCISW		((u32 *)(V7M_CACHE_MAINT_BASE + 0x10))
#define V7M_CACHE_REG_DCCMVAU		((u32 *)(V7M_CACHE_MAINT_BASE + 0x14))
#define V7M_CACHE_REG_DCCMVAC		((u32 *)(V7M_CACHE_MAINT_BASE + 0x18))
#define V7M_CACHE_REG_DCCSW		((u32 *)(V7M_CACHE_MAINT_BASE + 0x1C))
#define V7M_CACHE_REG_DCCIMVAC		((u32 *)(V7M_CACHE_MAINT_BASE + 0x20))
#define V7M_CACHE_REG_DCCISW		((u32 *)(V7M_CACHE_MAINT_BASE + 0x24))
#define WAYS_SHIFT			30
#define SETS_SHIFT			5

/* armv7m processor feature registers */

#define V7M_PROC_REG_CLIDR		((u32 *)(V7M_PROC_FTR_BASE + 0x00))
#define V7M_PROC_REG_CTR		((u32 *)(V7M_PROC_FTR_BASE + 0x04))
#define V7M_PROC_REG_CCSIDR		((u32 *)(V7M_PROC_FTR_BASE + 0x08))
#define MASK_NUM_WAYS			GENMASK(12, 3)
#define MASK_NUM_SETS			GENMASK(27, 13)
#define CLINE_SIZE_MASK			GENMASK(2, 0)
#define NUM_WAYS_SHIFT			3
#define NUM_SETS_SHIFT			13
#define V7M_PROC_REG_CSSELR		((u32 *)(V7M_PROC_FTR_BASE + 0x0C))
#define SEL_I_OR_D			BIT(0)

enum cache_type {
	DCACHE,
	ICACHE,
};

/* PoU : Point of Unification, Poc: Point of Coherency */
enum cache_action {
	INVALIDATE_POU,		/* i-cache invalidate by address */
	INVALIDATE_POC,		/* d-cache invalidate by address */
	INVALIDATE_SET_WAY,	/* d-cache invalidate by sets/ways */
	CLEAN_POU,		/* d-cache clean by address to the PoU */
	CLEAN_POC,		/* d-cache clean by address to the PoC */
	CLEAN_SET_WAY,		/* d-cache clean by sets/ways */
	CLEAN_INVAL_POC,	/* d-cache clean & invalidate by addr to PoC */
	CLEAN_INVAL_SET_WAY,	/* d-cache clean & invalidate by set/ways */
};

struct dcache_config {
	u32 ways;
	u32 sets;
};

static void get_cache_ways_sets(struct dcache_config *cache)
{
	u32 cache_size_id = readl(V7M_PROC_REG_CCSIDR);

	cache->ways = (cache_size_id & MASK_NUM_WAYS) >> NUM_WAYS_SHIFT;
	cache->sets = (cache_size_id & MASK_NUM_SETS) >> NUM_SETS_SHIFT;
}

/*
 * Return the io register to perform required cache action like clean or clean
 * & invalidate by sets/ways.
 */
static u32 *get_action_reg_set_ways(enum cache_action action)
{
	switch (action) {
	case INVALIDATE_SET_WAY:
		return V7M_CACHE_REG_DCISW;
	case CLEAN_SET_WAY:
		return V7M_CACHE_REG_DCCSW;
	case CLEAN_INVAL_SET_WAY:
		return V7M_CACHE_REG_DCCISW;
	default:
		break;
	};

	return NULL;
}

/*
 * Return the io register to perform required cache action like clean or clean
 * & invalidate by adddress or range.
 */
static u32 *get_action_reg_range(enum cache_action action)
{
	switch (action) {
	case INVALIDATE_POU:
		return V7M_CACHE_REG_ICIMVALU;
	case INVALIDATE_POC:
		return V7M_CACHE_REG_DCIMVAC;
	case CLEAN_POU:
		return V7M_CACHE_REG_DCCMVAU;
	case CLEAN_POC:
		return V7M_CACHE_REG_DCCMVAC;
	case CLEAN_INVAL_POC:
		return V7M_CACHE_REG_DCCIMVAC;
	default:
		break;
	}

	return NULL;
}

static u32 get_cline_size(enum cache_type type)
{
	u32 size;

	if (type == DCACHE)
		clrbits_le32(V7M_PROC_REG_CSSELR, BIT(SEL_I_OR_D));
	else if (type == ICACHE)
		setbits_le32(V7M_PROC_REG_CSSELR, BIT(SEL_I_OR_D));
	/* Make sure cache selection is effective for next memory access */
	dsb();

	size = readl(V7M_PROC_REG_CCSIDR) & CLINE_SIZE_MASK;
	/* Size enocoded as 2 less than log(no_of_words_in_cache_line) base 2 */
	size = 1 << (size + 2);
	debug("cache line size is %d\n", size);

	return size;
}

/* Perform the action like invalidate/clean on a range of cache addresses */
static int action_cache_range(enum cache_action action, u32 start_addr,
			      int64_t size)
{
	u32 cline_size;
	u32 *action_reg;
	enum cache_type type;

	action_reg = get_action_reg_range(action);
	if (!action_reg)
		return -EINVAL;
	if (action == INVALIDATE_POU)
		type = ICACHE;
	else
		type = DCACHE;

	/* Cache line size is minium size for the cache action */
	cline_size = get_cline_size(type);
	/* Align start address to cache line boundary */
	start_addr &= ~(cline_size - 1);
	debug("total size for cache action = %llx\n", size);
	do {
		writel(start_addr, action_reg);
		size -= cline_size;
		start_addr += cline_size;
	} while (size > cline_size);

	/* Make sure cache action is effective for next memory access */
	dsb();
	isb();	/* Make sure instruction stream sees it */
	debug("cache action on range done\n");

	return 0;
}

/* Perform the action like invalidate/clean on all cached addresses */
static int action_dcache_all(enum cache_action action)
{
	struct dcache_config cache;
	u32 *action_reg;
	int i, j;

	action_reg = get_action_reg_set_ways(action);
	if (!action_reg)
		return -EINVAL;

	clrbits_le32(V7M_PROC_REG_CSSELR, BIT(SEL_I_OR_D));
	/* Make sure cache selection is effective for next memory access */
	dsb();

	get_cache_ways_sets(&cache);	/* Get number of ways & sets */
	debug("cache: ways= %d, sets= %d\n", cache.ways + 1, cache.sets + 1);
	for (i = cache.sets; i >= 0; i--) {
		for (j = cache.ways; j >= 0; j--) {
			writel((j << WAYS_SHIFT) | (i << SETS_SHIFT),
			       action_reg);
		}
	}

	/* Make sure cache action is effective for next memory access */
	dsb();
	isb();	/* Make sure instruction stream sees it */

	return 0;
}

static int dcache_status(void)
{
	return (readl(&V7M_SCB->ccr) & BIT(V7M_CCR_DCACHE)) != 0;
}

static void dcache_enable(void)
{
	if (dcache_status())	/* return if cache already enabled */
		return;

	if (action_dcache_all(INVALIDATE_SET_WAY)) {
		pr_err("D-cache not enabled\n");
		return;
	}

	setbits_le32(&V7M_SCB->ccr, BIT(V7M_CCR_DCACHE));

	/* Make sure cache action is effective for next memory access */
	dsb();
	isb();	/* Make sure instruction stream sees it */
}

static void dcache_disable(void)
{
	if (!dcache_status())
		return;

	/* if dcache is enabled-> dcache disable & then flush */
	if (action_dcache_all(CLEAN_SET_WAY)) {
		pr_err("D-cache not flushed\n");
		return;
	}

	clrbits_le32(&V7M_SCB->ccr, BIT(V7M_CCR_DCACHE));

	/* Make sure cache action is effective for next memory access */
	dsb();
	isb();	/* Make sure instruction stream sees it */
}

void v7m_dma_inv_range(unsigned long start, unsigned long stop)
{
	if (action_cache_range(INVALIDATE_POC, start, stop - start)) {
		pr_err("D-cache not invalidated\n");
		return;
	}
}

void v7m_dma_clean_range(unsigned long start, unsigned long stop)
{
	if (action_cache_range(CLEAN_POC, start, stop - start)) {
		pr_err("D-cache not flushed\n");
		return;
	}
}

void v7m_dma_flush_range(unsigned long start, unsigned long stop)
{
	if (action_cache_range(CLEAN_INVAL_POC, start, stop - start)) {
		pr_err("D-cache not flushed\n");
		return;
	}
}

static void flush_dcache_all(void)
{
	if (action_dcache_all(CLEAN_SET_WAY)) {
		pr_err("D-cache not flushed\n");
		return;
	}
}

static void invalidate_dcache_all(void)
{
	if (action_dcache_all(INVALIDATE_SET_WAY)) {
		pr_err("D-cache not invalidated\n");
		return;
	}
}

void v7m_invalidate_icache_all(void)
{
	writel(INVAL_ICACHE_POU, V7M_CACHE_REG_ICIALLU);

	/* Make sure cache action is effective for next memory access */
	dsb();
	isb();	/* Make sure instruction stream sees it */
}

static int icache_status(void)
{
	return (readl(&V7M_SCB->ccr) & BIT(V7M_CCR_ICACHE)) != 0;
}

static void icache_enable(void)
{
	if (icache_status())
		return;

	v7m_invalidate_icache_all();
	setbits_le32(&V7M_SCB->ccr, BIT(V7M_CCR_ICACHE));

	/* Make sure cache action is effective for next memory access */
	dsb();
	isb();	/* Make sure instruction stream sees it */
}

static void icache_disable(void)
{
	if (!icache_status())
		return;

	isb();	/* flush pipeline */
	clrbits_le32(&V7M_SCB->ccr, BIT(V7M_CCR_ICACHE));
	isb();	/* subsequent instructions fetch see cache disable effect */
}

void v7m_mmu_cache_on(void)
{
	icache_enable();
	dcache_enable();
}

void v7m_mmu_cache_off(void)
{
	dcache_disable();
	/* invalidate to make sure no cache line gets dirty between
	 * dcache flushing and disabling dcache */
	invalidate_dcache_all();

	icache_disable();
	v7m_invalidate_icache_all();
}

void v7m_mmu_cache_flush(void)
{
	flush_dcache_all();
	v7m_invalidate_icache_all();
}

void v7m_mmu_cache_invalidate(void)
{
	invalidate_dcache_all();
	v7m_invalidate_icache_all();
}
// SPDX-License-Identifier: GPL-2.0-only
// SPDX-FileCopyrightText: 2010-2013 Sascha Hauer <s.hauer@pengutronix.de>, Pengutronix
// SPDX-FileCopyrightText: 2012 Jean-Christophe PLAGNIOL-VILLARD <plagnioj@jcrosoft.com>

/* uncompress.c - uncompressor code for self extracing pbl image */

#define pr_fmt(fmt) "uncompress.c: " fmt

#include <common.h>
#include <init.h>
#include <linux/sizes.h>
#include <pbl.h>
#include <asm/barebox-arm.h>
#include <asm/barebox-arm-head.h>
#include <asm-generic/memory_layout.h>
#include <asm/sections.h>
#include <asm/secure.h>
#include <asm/cache.h>
#include <asm/mmu.h>
#include <asm/unaligned.h>

#include <debug_ll.h>

#include "entry.h"

#ifndef CONFIG_HAVE_PBL_MULTI_IMAGES

void start_pbl(void);

/*
 * First instructions in the pbl image
 */
void __naked __section(.text_head_entry_start_single_pbl) start_pbl(void)
{
	barebox_arm_head();
}
#endif

unsigned long free_mem_ptr;
unsigned long free_mem_end_ptr;

extern unsigned char input_data[];
extern unsigned char input_data_end[];

void __noreturn barebox_pbl_start(unsigned long membase, unsigned long memsize,
				  void *boarddata)
{
	uint32_t pg_len, uncompressed_len;
	void __noreturn (*barebox)(unsigned long, unsigned long, void *);
	unsigned long endmem = membase + memsize;
	unsigned long barebox_base;
	void *pg_start, *pg_end;
	unsigned long pc = get_pc();

	/* piggy data is not relocated, so determine the bounds now */
	pg_start = input_data + global_variable_offset();
	pg_end = input_data_end + global_variable_offset();

	if (IS_ENABLED(CONFIG_PBL_RELOCATABLE) && !IS_ENABLED(CONFIG_PBL_XIP)) {
		/*
		 * If we run from inside the memory just relocate the binary
		 * to the current address. Otherwise it may be a readonly location.
		 * Copy and relocate to the start of the memory in this case.
		 */
		if (pc > membase && pc - membase < memsize)
			relocate_to_current_adr();
		else
			relocate_to_adr(membase);
	}

	pg_len = pg_end - pg_start;
	uncompressed_len = get_unaligned((const u32 *)(pg_start + pg_len - 4));

	if (IS_ENABLED(CONFIG_RELOCATABLE))
		barebox_base = arm_mem_barebox_image(membase, endmem,
						     uncompressed_len + MAX_BSS_SIZE);
	else
		barebox_base = TEXT_BASE;

	setup_c();

	pr_debug("memory at 0x%08lx, size 0x%08lx\n", membase, memsize);

	if (IS_ENABLED(CONFIG_MMU_EARLY)) {
		unsigned long ttb = arm_mem_ttb(membase, endmem);
		pr_debug("enabling MMU, ttb @ 0x%08lx\n", ttb);
		mmu_early_enable(membase, memsize, ttb);
	}

	free_mem_ptr = arm_mem_early_malloc(membase, endmem);
	free_mem_end_ptr = arm_mem_early_malloc_end(membase, endmem);

	pr_debug("uncompressing barebox binary at 0x%p (size 0x%08x) to 0x%08lx (uncompressed size: 0x%08x)\n",
			pg_start, pg_len, barebox_base, uncompressed_len);

	pbl_barebox_uncompress((void*)barebox_base, pg_start, pg_len);

	sync_caches_for_execution();

	if (IS_ENABLED(CONFIG_THUMB2_BAREBOX))
		barebox = (void *)(barebox_base + 1);
	else
		barebox = (void *)barebox_base;

	pr_debug("jumping to uncompressed image at 0x%p\n", barebox);

	if (IS_ENABLED(CONFIG_CPU_V7) && boot_cpu_mode() == HYP_MODE)
		armv7_switch_to_hyp();

	barebox(membase, memsize, boarddata);
}

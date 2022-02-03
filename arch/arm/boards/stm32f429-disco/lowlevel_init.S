/* SPDX-License-Identifier: GPL-2.0-only */

#include <config.h>
#include <asm-generic/memory_layout.h>
#include <asm/barebox-arm-head.h>
#include <linux/sizes.h>
#include <mach/stm32.h>

#define SIZE_192K (SZ_128K + SZ_64K)

#define LOAD_OFFSET STM32F_FLASH_BASE

.section ".text_head_entry_start_stm32f439_disco","ax"
.globl start_stm32f429_disco
start_stm32f429_disco:
	.int	0x10010000 /* initial stack pointer */
	.int	LOAD_OFFSET + continue_stm32f429_disco
	.int	LOAD_OFFSET + nmi
	.int	LOAD_OFFSET + hard_fault
	.int	LOAD_OFFSET + mm_fault
	.int	LOAD_OFFSET + bus_fault
	.int	LOAD_OFFSET + usage_fault
	.rept	100
	.int	LOAD_OFFSET + loop
	.endr
nmi:
	b nmi
hard_fault:
	b hard_fault
mm_fault:
	b mm_fault
bus_fault:
	b bus_fault
usage_fault:
	b usage_fault
loop:
	b loop

/* SPDX-License-Identifier: GPL-2.0-or-later */

/*
 * ARM Cortex M3/M4/M7 SysTick timer driver
 * (C) Copyright 2017 Renesas Electronics Europe Ltd
 *
 * Based on arch/arm/mach-stm32/stm32f1/timer.c
 * (C) Copyright 2015
 * Kamil Lulko, <kamil.lulko@gmail.com>
 *
 * Copyright 2015 ATS Advanced Telematics Systems GmbH
 * Copyright 2015 Konsulko Group, Matt Porter <mporter@konsulko.com>
 *
 * The SysTick timer is a 24-bit count down timer. The clock can be either the
 * CPU clock or a reference clock. The timer will wrap around very quickly
 * when using the CPU clock, and we do not handle the timer interrupts, it is
 * expected that this driver is only ever used early on with short delays.
 *
 * The number of reference clock ticks that correspond to 10ms is normally
 * defined in the SysTick Calibration register's TENMS field. However, on some
 * devices this is wrong, for these __early_udelay can be used.
 */

#ifndef __MACH_V7M_EARLY_UDELAY_H_
#define __MACH_V7M_EARLY_UDELAY_H_

#include <linux/types.h>
#include <linux/bitops.h>
#include <linux/kernel.h>
#include <io.h>

/* SysTick Base Address - fixed for all Cortex M3, M4 and M7 devices */
#define SYSTICK_BASE		IOMEM(0xE000E010)

struct cm3_systick {
	u32 ctrl;
	u32 reload_val;
	u32 current_val;
	u32 calibration;
};

#define SYSTICK_CTRL_EN		BIT(0)
#define SYSTICK_CTRL_CNT_FLG	BIT(16)
/* Clock source: 0 = Ref clock, 1 = CPU clock */
#define SYSTICK_CTRL_CPU_CLK	BIT(2)
#define SYSTICK_CAL_NOREF	BIT(31)
#define SYSTICK_CAL_SKEW	BIT(30)
#define SYSTICK_LOAD_RELOAD_MASK 0x00FFFFFF

/*
 * Call this directly first if the TENMS field is inexact or wrong
 */
static inline u32 early_udelay_init(u32 timer_rate_hz)
{
	struct cm3_systick __iomem *systick = SYSTICK_BASE;
	u32 cal;

	cal = readl(&systick->calibration);
	if (!timer_rate_hz)
		timer_rate_hz = (cal & SYSTICK_LOAD_RELOAD_MASK) * 100;

	if (cal & SYSTICK_CAL_NOREF)
		/* Use CPU clock, no interrupts */
		writel(SYSTICK_CTRL_EN | SYSTICK_CTRL_CPU_CLK, &systick->ctrl);
	else
		/* Use external clock, no interrupts */
		writel(SYSTICK_CTRL_EN, &systick->ctrl);

	writel(timer_rate_hz, &systick->reload_val);

	return timer_rate_hz;
}

static inline u32 early_udelay(u32 usecs)
{
	struct cm3_systick __iomem *systick = SYSTICK_BASE;
	u32 timer_rate_hz, ticks;

	timer_rate_hz = readl(&systick->reload_val);
	if (!timer_rate_hz)
		timer_rate_hz = early_udelay_init(0);

	ticks = mult_frac(timer_rate_hz, usecs, 1000 * 1000);
	if (!ticks)
		return 0;

	writel(ticks, &systick->reload_val);
	/* Any write to current_val reg clears it to 0 */
	writel(0, &systick->current_val);

	while (!(readl(&systick->ctrl) & SYSTICK_CTRL_CNT_FLG))
		;

	writel(timer_rate_hz, &systick->reload_val);

	return ticks;
}

#endif

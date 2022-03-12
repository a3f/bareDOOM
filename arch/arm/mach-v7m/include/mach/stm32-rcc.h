/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef STM32_RCC_H_
#define STM32_RCC_H_

#include <mach/stm32.h>
#include <linux/types.h>
#include <linux/bitops.h>
#include <io.h>

static inline void rcc_gpio_clock_enable(void)
{
	setbits_le32(IOMEM(STM32_RCC_BASE) + 0x30, 0xff);
}

static inline void rcc_fmc_clock_enable(void)
{
	setbits_le32(IOMEM(STM32_RCC_BASE + 0x38), BIT(0)); /* ungate FMC */
}

int stm32f42x_pll_init(u32 hse_rate_mhz);
int stm32f469_pll_init(u32 hse_rate_mhz);
int stm32f7_pll_init(u32 hse_rate_mhz);

#endif

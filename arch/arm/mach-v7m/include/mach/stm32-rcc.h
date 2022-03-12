/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef STM32_RCC_H_
#define STM32_RCC_H_

#include <mach/stm32.h>
#include <linux/types.h>
#include <io.h>

#define _REG_BIT(base, bit)		(((base) << 5) + (bit))

enum rcc_periph_clken {
	RCC_GPIOA	= _REG_BIT(0x30, 0),
	RCC_GPIOB	= _REG_BIT(0x30, 1),
	RCC_GPIOC	= _REG_BIT(0x30, 2),
	RCC_GPIOD	= _REG_BIT(0x30, 3),
	RCC_GPIOE	= _REG_BIT(0x30, 4),
	RCC_GPIOF	= _REG_BIT(0x30, 5),
	RCC_GPIOG	= _REG_BIT(0x30, 6),
	RCC_FMC         = _REG_BIT(0x38, 0),
};

#define RCC_REG(i)	IOMEM(STM32_RCC_BASE + ((i) >> 5))
#define RCC_BIT(i)	(1 << ((i) & 0x1f))

static inline void rcc_periph_clock_enable(enum rcc_periph_clken clken)
{
	setbits_le32(RCC_REG(clken), RCC_BIT(clken));
}

static inline void rcc_gpio_clock_enable(void)
{
	setbits_le32(IOMEM(STM32_RCC_BASE) + 0x30, 0xff);
}

int stm32f42x_pll_init(u32 hse_rate_mhz);
int stm32f469_pll_init(u32 hse_rate_mhz);
int stm32f7_pll_init(u32 hse_rate_mhz);

#endif

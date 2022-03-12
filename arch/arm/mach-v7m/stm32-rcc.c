// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (C) 2017, STMicroelectronics - All Rights Reserved
 * Author(s): Vikas Manocha, <vikas.manocha@st.com> for STMicroelectronics.
 */

#include <mach/stm32.h>
#include <mach/stm32-rcc.h>
#include <debug_ll.h>
#include <io.h>
#include <linux/bitops.h>

#define RCC_CR_HSION			BIT(0)
#define RCC_CR_HSEON			BIT(16)
#define RCC_CR_HSERDY			BIT(17)
#define RCC_CR_HSEBYP			BIT(18)
#define RCC_CR_CSSON			BIT(19)
#define RCC_CR_PLLON			BIT(24)
#define RCC_CR_PLLRDY			BIT(25)
#define RCC_CR_PLLSAION			BIT(28)
#define RCC_CR_PLLSAIRDY		BIT(29)

#define RCC_PLLCFGR_PLLM_MASK		GENMASK(5, 0)
#define RCC_PLLCFGR_PLLN_MASK		GENMASK(14, 6)
#define RCC_PLLCFGR_PLLP_MASK		GENMASK(17, 16)
#define RCC_PLLCFGR_PLLQ_MASK		GENMASK(27, 24)
#define RCC_PLLCFGR_PLLSRC		BIT(22)
#define RCC_PLLCFGR_PLLM_SHIFT		0
#define RCC_PLLCFGR_PLLN_SHIFT		6
#define RCC_PLLCFGR_PLLP_SHIFT		16
#define RCC_PLLCFGR_PLLQ_SHIFT		24

#define RCC_CFGR_AHB_PSC_MASK		GENMASK(7, 4)
#define RCC_CFGR_APB1_PSC_MASK		GENMASK(12, 10)
#define RCC_CFGR_APB2_PSC_MASK		GENMASK(15, 13)
#define RCC_CFGR_SW0			BIT(0)
#define RCC_CFGR_SW1			BIT(1)
#define RCC_CFGR_SW_MASK		GENMASK(1, 0)
#define RCC_CFGR_SW_HSI			0
#define RCC_CFGR_SW_HSE			RCC_CFGR_SW0
#define RCC_CFGR_SW_PLL			RCC_CFGR_SW1
#define RCC_CFGR_SWS0			BIT(2)
#define RCC_CFGR_SWS1			BIT(3)
#define RCC_CFGR_SWS_MASK		GENMASK(3, 2)
#define RCC_CFGR_SWS_HSI		0
#define RCC_CFGR_SWS_HSE		RCC_CFGR_SWS0
#define RCC_CFGR_SWS_PLL		RCC_CFGR_SWS1
#define RCC_CFGR_HPRE_SHIFT		4
#define RCC_CFGR_PPRE1_SHIFT		10
#define RCC_CFGR_PPRE2_SHIFT		13

#define RCC_PLLSAICFGR_PLLSAIN_MASK	GENMASK(14, 6)
#define RCC_PLLSAICFGR_PLLSAIP_MASK	GENMASK(17, 16)
#define RCC_PLLSAICFGR_PLLSAIQ_MASK	GENMASK(27, 24)
#define RCC_PLLSAICFGR_PLLSAIR_MASK	GENMASK(30, 28)
#define RCC_PLLSAICFGR_PLLSAIN_SHIFT	6
#define RCC_PLLSAICFGR_PLLSAIP_SHIFT	16
#define RCC_PLLSAICFGR_PLLSAIQ_SHIFT	24
#define RCC_PLLSAICFGR_PLLSAIR_SHIFT	28
#define RCC_PLLSAICFGR_PLLSAIP_4	BIT(16)
#define RCC_PLLSAICFGR_PLLSAIQ_4	BIT(26)
#define RCC_PLLSAICFGR_PLLSAIR_3	BIT(29) | BIT(28)

#define RCC_DCKCFGRX_TIMPRE		BIT(24)
#define RCC_DCKCFGRX_CK48MSEL		BIT(27)
#define RCC_DCKCFGRX_SDMMC1SEL		BIT(28)
#define RCC_DCKCFGR2_SDMMC2SEL		BIT(29)

#define RCC_DCKCFGR_PLLSAIDIVR_SHIFT    16
#define RCC_DCKCFGR_PLLSAIDIVR_MASK	GENMASK(17, 16)
#define RCC_DCKCFGR_PLLSAIDIVR_2	0

/*
 * RCC AHB1ENR specific definitions
 */
#define RCC_AHB1ENR_ETHMAC_EN		BIT(25)
#define RCC_AHB1ENR_ETHMAC_TX_EN	BIT(26)
#define RCC_AHB1ENR_ETHMAC_RX_EN	BIT(27)

/*
 * RCC APB1ENR specific definitions
 */
#define RCC_APB1ENR_TIM2EN		BIT(0)
#define RCC_APB1ENR_PWREN		BIT(28)

/*
 * RCC APB2ENR specific definitions
 */
#define RCC_APB2ENR_SYSCFGEN		BIT(14)
#define RCC_APB2ENR_SAI1EN		BIT(22)

#define AHB_PSC_1			0
#define AHB_PSC_2			0x8
#define AHB_PSC_4			0x9
#define AHB_PSC_8			0xA
#define AHB_PSC_16			0xB
#define AHB_PSC_64			0xC
#define AHB_PSC_128			0xD
#define AHB_PSC_256			0xE
#define AHB_PSC_512			0xF

#define APB_PSC_1			0
#define APB_PSC_2			0x4
#define APB_PSC_4			0x5
#define APB_PSC_8			0x6
#define APB_PSC_16			0x7

/*
 * Offsets of some PWR registers
 */
#define PWR_CR1_ODEN			BIT(16)
#define PWR_CR1_ODSWEN			BIT(17)
#define PWR_CSR1_ODRDY			BIT(16)
#define PWR_CSR1_ODSWRDY		BIT(17)

struct stm32_pwr_regs {
	u32 cr1;   /* power control register 1 */
	u32 csr1;  /* power control/status register 2 */
	u32 cr2;   /* power control register 2 */
	u32 csr2;  /* power control/status register 2 */
};

struct pll_psc {
	u16	pll_n;
	u8	pll_p;
	u8	pll_q;
	u8	ahb_psc;
	u8	apb1_psc;
	u8	apb2_psc;
};

struct stm32_clk_info {
	struct pll_psc sys_pll_psc;
	bool has_overdrive;
	bool v2;
};

enum soc_family {
	STM32F42X,
	STM32F469,
	STM32F7,
	STM32MP1,
};

enum apb {
	APB1,
	APB2,
};

struct stm32_rcc_clk {
	char *drv_name;
	enum soc_family soc;
};

struct stm32_rcc_regs {
	u32 cr;		/* RCC clock control */
	u32 pllcfgr;	/* RCC PLL configuration */
	u32 cfgr;	/* RCC clock configuration */
	u32 cir;	/* RCC clock interrupt */
	u32 ahb1rstr;	/* RCC AHB1 peripheral reset */
	u32 ahb2rstr;	/* RCC AHB2 peripheral reset */
	u32 ahb3rstr;	/* RCC AHB3 peripheral reset */
	u32 rsv0;
	u32 apb1rstr;	/* RCC APB1 peripheral reset */
	u32 apb2rstr;	/* RCC APB2 peripheral reset */
	u32 rsv1[2];
	u32 ahb1enr;	/* RCC AHB1 peripheral clock enable */
	u32 ahb2enr;	/* RCC AHB2 peripheral clock enable */
	u32 ahb3enr;	/* RCC AHB3 peripheral clock enable */
	u32 rsv2;
	u32 apb1enr;	/* RCC APB1 peripheral clock enable */
	u32 apb2enr;	/* RCC APB2 peripheral clock enable */
	u32 rsv3[2];
	u32 ahb1lpenr;	/* RCC AHB1 periph clk enable in low pwr mode */
	u32 ahb2lpenr;	/* RCC AHB2 periph clk enable in low pwr mode */
	u32 ahb3lpenr;	/* RCC AHB3 periph clk enable in low pwr mode */
	u32 rsv4;
	u32 apb1lpenr;	/* RCC APB1 periph clk enable in low pwr mode */
	u32 apb2lpenr;	/* RCC APB2 periph clk enable in low pwr mode */
	u32 rsv5[2];
	u32 bdcr;	/* RCC Backup domain control */
	u32 csr;	/* RCC clock control & status */
	u32 rsv6[2];
	u32 sscgr;	/* RCC spread spectrum clock generation */
	u32 plli2scfgr;	/* RCC PLLI2S configuration */
	/* below registers are only available on STM32F46x and STM32F7 SoCs*/
	u32 pllsaicfgr;	/* PLLSAI configuration */
	u32 dckcfgr;	/* dedicated clocks configuration register */
	/* Below registers are only available on STM32F7 SoCs */
	u32 dckcfgr2;	/* dedicated clocks configuration register */
};

struct stm32_clk {
	struct stm32_rcc_regs *base;
	struct stm32_pwr_regs *pwr_regs;
	const struct stm32_clk_info *info;
	unsigned long hse_rate_mhz;
	bool pllsaip;
};

#define FLASH_ACR_PRFTEN	(1 << 8)
#define FLASH_ACR_ICEN		(1 << 9)
#define FLASH_ACR_DCEN		(1 << 10)

static void stm32_flash_latency_cfg(int latency)
{
	/* 5 wait states, Prefetch enabled, D-Cache enabled, I-Cache enabled */
	writel(latency | FLASH_ACR_PRFTEN | FLASH_ACR_ICEN | FLASH_ACR_DCEN,
	       STM32_FLASH_CNTL_BASE);
}

static int configure_clocks(struct stm32_clk *priv)
{
	struct stm32_rcc_regs *regs = priv->base;
	struct stm32_pwr_regs *pwr = priv->pwr_regs;
	const struct pll_psc *sys_pll_psc = &priv->info->sys_pll_psc;

	/* Reset RCC configuration */
	setbits_le32(&regs->cr, RCC_CR_HSION);
	writel(0, &regs->cfgr); /* Reset CFGR */
	clrbits_le32(&regs->cr, (RCC_CR_HSEON | RCC_CR_CSSON
		| RCC_CR_PLLON | RCC_CR_PLLSAION));
	writel(0x24003010, &regs->pllcfgr); /* Reset value from RM */
	clrbits_le32(&regs->cr, RCC_CR_HSEBYP);
	writel(0, &regs->cir); /* Disable all interrupts */

	/* Configure for HSE+PLL operation */
	setbits_le32(&regs->cr, RCC_CR_HSEON);
	while (!(readl(&regs->cr) & RCC_CR_HSERDY))
		;

	setbits_le32(&regs->cfgr, ((
		sys_pll_psc->ahb_psc << RCC_CFGR_HPRE_SHIFT)
		| (sys_pll_psc->apb1_psc << RCC_CFGR_PPRE1_SHIFT)
		| (sys_pll_psc->apb2_psc << RCC_CFGR_PPRE2_SHIFT)));

	/* Configure the main PLL */
	setbits_le32(&regs->pllcfgr, RCC_PLLCFGR_PLLSRC); /* pll source HSE */
	clrsetbits_le32(&regs->pllcfgr, RCC_PLLCFGR_PLLM_MASK,
			priv->hse_rate_mhz << RCC_PLLCFGR_PLLM_SHIFT);
	clrsetbits_le32(&regs->pllcfgr, RCC_PLLCFGR_PLLN_MASK,
			sys_pll_psc->pll_n << RCC_PLLCFGR_PLLN_SHIFT);
	clrsetbits_le32(&regs->pllcfgr, RCC_PLLCFGR_PLLP_MASK,
			((sys_pll_psc->pll_p >> 1) - 1) << RCC_PLLCFGR_PLLP_SHIFT);
	clrsetbits_le32(&regs->pllcfgr, RCC_PLLCFGR_PLLQ_MASK,
			sys_pll_psc->pll_q << RCC_PLLCFGR_PLLQ_SHIFT);

	/* configure SDMMC clock */
	if (priv->info->v2) { /*stm32f7 case */
		if (priv->pllsaip)
			/* select PLLSAIP as 48MHz clock source */
			setbits_le32(&regs->dckcfgr2, RCC_DCKCFGRX_CK48MSEL);
		else
			/* select PLLQ as 48MHz clock source */
			clrbits_le32(&regs->dckcfgr2, RCC_DCKCFGRX_CK48MSEL);

		/* select 48MHz as SDMMC1 clock source */
		clrbits_le32(&regs->dckcfgr2, RCC_DCKCFGRX_SDMMC1SEL);

		/* select 48MHz as SDMMC2 clock source */
		clrbits_le32(&regs->dckcfgr2, RCC_DCKCFGR2_SDMMC2SEL);
	} else  { /* stm32f4 case */
		if (priv->pllsaip)
			/* select PLLSAIP as 48MHz clock source */
			setbits_le32(&regs->dckcfgr, RCC_DCKCFGRX_CK48MSEL);
		else
			/* select PLLQ as 48MHz clock source */
			clrbits_le32(&regs->dckcfgr, RCC_DCKCFGRX_CK48MSEL);

		/* select 48MHz as SDMMC1 clock source */
		clrbits_le32(&regs->dckcfgr, RCC_DCKCFGRX_SDMMC1SEL);
	}

	/*
	 * Configure the SAI PLL to generate LTDC pixel clock and
	 * 48 Mhz for SDMMC and USB
	 */
	clrsetbits_le32(&regs->pllsaicfgr, RCC_PLLSAICFGR_PLLSAIP_MASK,
			RCC_PLLSAICFGR_PLLSAIP_4);
	clrsetbits_le32(&regs->pllsaicfgr, RCC_PLLSAICFGR_PLLSAIR_MASK,
			RCC_PLLSAICFGR_PLLSAIR_3);
	clrsetbits_le32(&regs->pllsaicfgr, RCC_PLLSAICFGR_PLLSAIN_MASK,
			195 << RCC_PLLSAICFGR_PLLSAIN_SHIFT);

	clrsetbits_le32(&regs->dckcfgr, RCC_DCKCFGR_PLLSAIDIVR_MASK,
			RCC_DCKCFGR_PLLSAIDIVR_2 << RCC_DCKCFGR_PLLSAIDIVR_SHIFT);

	/* Enable the main PLL */
	setbits_le32(&regs->cr, RCC_CR_PLLON);
	while (!(readl(&regs->cr) & RCC_CR_PLLRDY))
		;

	/* Enable the SAI PLL */
	setbits_le32(&regs->cr, RCC_CR_PLLSAION);
	while (!(readl(&regs->cr) & RCC_CR_PLLSAIRDY))
		;
	setbits_le32(&regs->apb1enr, RCC_APB1ENR_PWREN);

	if (priv->info->has_overdrive) {
		/*
		 * Enable high performance mode
		 * System frequency up to 200 MHz
		 */
		setbits_le32(&pwr->cr1, PWR_CR1_ODEN);
		/* Infinite wait! */
		while (!(readl(&pwr->csr1) & PWR_CSR1_ODRDY))
			;
		/* Enable the Over-drive switch */
		setbits_le32(&pwr->cr1, PWR_CR1_ODSWEN);
		/* Infinite wait! */
		while (!(readl(&pwr->csr1) & PWR_CSR1_ODSWRDY))
			;
	}

	stm32_flash_latency_cfg(5);
	clrbits_le32(&regs->cfgr, (RCC_CFGR_SW0 | RCC_CFGR_SW1));
	setbits_le32(&regs->cfgr, RCC_CFGR_SW_PLL);

	while ((readl(&regs->cfgr) & RCC_CFGR_SWS_MASK) != RCC_CFGR_SWS_PLL)
		;

	/* gate the SYSCFG clock, needed to set RMII ethernet interface */
	setbits_le32(&regs->apb2enr, RCC_APB2ENR_SYSCFGEN);

	return 0;
}

static const struct stm32_clk stm32f_priv = {
	.base = IOMEM(0x40023800),
	.pwr_regs = IOMEM(0x40007000),
	.pllsaip = true,
};

static const struct stm32_clk_info stm32f4_clk_info = {
	/* 180 MHz */
	.sys_pll_psc = {
		.pll_n = 360,
		.pll_p = 2,
		.pll_q = 8,
		.ahb_psc = AHB_PSC_1,
		.apb1_psc = APB_PSC_4,
		.apb2_psc = APB_PSC_2,
	},
	.has_overdrive = false,
	.v2 = false,
};

int stm32f42x_pll_init(u32 hse_rate_mhz)
{
	struct stm32_clk priv = stm32f_priv;

	priv.info = &stm32f4_clk_info;
	priv.hse_rate_mhz = hse_rate_mhz;
	priv.pllsaip = false;

	return configure_clocks(&priv);
}

int stm32f469_pll_init(u32 hse_rate_mhz)
{
	struct stm32_clk priv = stm32f_priv;

	priv.info = &stm32f4_clk_info;
	priv.hse_rate_mhz = hse_rate_mhz;

	return configure_clocks(&priv);
}

static const struct stm32_clk_info stm32f7_clk_info = {
	/* 200 MHz */
	.sys_pll_psc = {
		.pll_n = 400,
		.pll_p = 2,
		.pll_q = 8,
		.ahb_psc = AHB_PSC_1,
		.apb1_psc = APB_PSC_4,
		.apb2_psc = APB_PSC_2,
	},
	.has_overdrive = true,
	.v2 = true,
};

int stm32f7_pll_init(u32 hse_rate_mhz)
{
	struct stm32_clk priv = stm32f_priv;

	priv.info = &stm32f7_clk_info;
	priv.hse_rate_mhz = hse_rate_mhz;

	return configure_clocks(&priv);
}

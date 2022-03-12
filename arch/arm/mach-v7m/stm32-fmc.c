// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (C) 2017, STMicroelectronics - All Rights Reserved
 * Author(s): Vikas Manocha, <vikas.manocha@st.com> for STMicroelectronics.
 */

#include <io.h>
#include <linux/bitops.h>
#include <mach/stm32-rcc.h>
#include <mach/stm32-fmc.h>
#include <mach/early_udelay.h>
#include <debug_ll.h>

struct stm32_fmc_regs {
	/* 0x0 */
	u32 bcr1;	/* NOR/PSRAM Chip select control register 1 */
	u32 btr1;	/* SRAM/NOR-Flash Chip select timing register 1 */
	u32 bcr2;	/* NOR/PSRAM Chip select Control register 2 */
	u32 btr2;	/* SRAM/NOR-Flash Chip select timing register 2 */
	u32 bcr3;	/* NOR/PSRAMChip select Control register 3 */
	u32 btr3;	/* SRAM/NOR-Flash Chip select timing register 3 */
	u32 bcr4;	/* NOR/PSRAM Chip select Control register 4 */
	u32 btr4;	/* SRAM/NOR-Flash Chip select timing register 4 */
	u32 reserved1[24];

	/* 0x80 */
	u32 pcr;	/* NAND Flash control register */
	u32 sr;		/* FIFO status and interrupt register */
	u32 pmem;	/* Common memory space timing register */
	u32 patt;	/* Attribute memory space timing registers  */
	u32 reserved2[1];
	u32 eccr;	/* ECC result registers */
	u32 reserved3[27];

	/* 0x104 */
	u32 bwtr1;	/* SRAM/NOR-Flash write timing register 1 */
	u32 reserved4[1];
	u32 bwtr2;	/* SRAM/NOR-Flash write timing register 2 */
	u32 reserved5[1];
	u32 bwtr3;	/* SRAM/NOR-Flash write timing register 3 */
	u32 reserved6[1];
	u32 bwtr4;	/* SRAM/NOR-Flash write timing register 4 */
	u32 reserved7[8];

	/* 0x140 */
	u32 sdcr1;	/* SDRAM Control register 1 */
	u32 sdcr2;	/* SDRAM Control register 2 */
	u32 sdtr1;	/* SDRAM Timing register 1 */
	u32 sdtr2;	/* SDRAM Timing register 2 */
	u32 sdcmr;	/* SDRAM Mode register */
	u32 sdrtr;	/* SDRAM Refresh timing register */
	u32 sdsr;	/* SDRAM Status register */
};

/*
 * NOR/PSRAM Control register BCR1
 * FMC controller Enable, only availabe for H7
 */
#define FMC_BCR1_FMCEN		BIT(31)

/* Control register SDCR */
#define FMC_SDCR_RPIPE_SHIFT	13	/* RPIPE bit shift */
#define FMC_SDCR_RBURST_SHIFT	12	/* RBURST bit shift */
#define FMC_SDCR_SDCLK_SHIFT	10	/* SDRAM clock divisor shift */
#define FMC_SDCR_WP_SHIFT	9	/* Write protection shift */
#define FMC_SDCR_CAS_SHIFT	7	/* CAS latency shift */
#define FMC_SDCR_NB_SHIFT	6	/* Number of banks shift */
#define FMC_SDCR_MWID_SHIFT	4	/* Memory width shift */
#define FMC_SDCR_NR_SHIFT	2	/* Number of row address bits shift */
#define FMC_SDCR_NC_SHIFT	0	/* Number of col address bits shift */

/* Timings register SDTR */
#define FMC_SDTR_TMRD_SHIFT	0	/* Load mode register to active */
#define FMC_SDTR_TXSR_SHIFT	4	/* Exit self-refresh time */
#define FMC_SDTR_TRAS_SHIFT	8	/* Self-refresh time */
#define FMC_SDTR_TRC_SHIFT	12	/* Row cycle delay */
#define FMC_SDTR_TWR_SHIFT	16	/* Recovery delay */
#define FMC_SDTR_TRP_SHIFT	20	/* Row precharge delay */
#define FMC_SDTR_TRCD_SHIFT	24	/* Row-to-column delay */

#define FMC_SDCMR_NRFS_SHIFT	5

#define FMC_SDCMR_MODE_NORMAL		0
#define FMC_SDCMR_MODE_START_CLOCK	1
#define FMC_SDCMR_MODE_PRECHARGE	2
#define FMC_SDCMR_MODE_AUTOREFRESH	3
#define FMC_SDCMR_MODE_WRITE_MODE	4
#define FMC_SDCMR_MODE_SELFREFRESH	5
#define FMC_SDCMR_MODE_POWERDOWN	6

#define FMC_SDCMR_BANK_1		BIT(4)
#define FMC_SDCMR_BANK_2		BIT(3)

#define FMC_SDCMR_MODE_REGISTER_SHIFT	9

#define FMC_SDSR_BUSY			BIT(5)

#define FMC_BUSY_WAIT(regs)	do {                         \
		__asm__ __volatile__ ("dsb" : : : "memory"); \
		while (regs->sdsr & FMC_SDSR_BUSY)           \
			;                                    \
	} while (0)

#define SDRAM_MODE_BL_SHIFT	0
#define SDRAM_MODE_CAS_SHIFT	4
#define SDRAM_MODE_BL		0

static void udelay(u32 usecs)
{
	/* wait twice as long as safety margin for inaccurate clock */
	early_udelay(usecs * 2);
}

static int stm32_init_fmc(struct stm32_fmc_regs __iomem *regs,
			  const struct stm32_bank_params *bank_params)
{
	const struct stm32_sdram_control *control;
	const struct stm32_sdram_timing *timing;
	enum stm32_fmc_bank target_bank;
	u32 ctb; /* SDCMR register: Command Target Bank */
	u32 ref_count;

	//barebox_rcc_periph_clock_enable(RCC_FMC);

	control = &bank_params->sdram_control;
	timing = &bank_params->sdram_timing;
	target_bank = bank_params->target_bank;
	ref_count = bank_params->sdram_ref_count;

	writel(control->sdclk << FMC_SDCR_SDCLK_SHIFT
		| control->cas_latency << FMC_SDCR_CAS_SHIFT
		| control->no_banks << FMC_SDCR_NB_SHIFT
		| control->memory_width << FMC_SDCR_MWID_SHIFT
		| control->no_rows << FMC_SDCR_NR_SHIFT
		| control->no_columns << FMC_SDCR_NC_SHIFT
		| control->rd_pipe_delay << FMC_SDCR_RPIPE_SHIFT
		| control->rd_burst << FMC_SDCR_RBURST_SHIFT,
		&regs->sdcr1);

	if (target_bank == SDRAM_BANK2)
		writel(control->cas_latency << FMC_SDCR_CAS_SHIFT
			| control->no_banks << FMC_SDCR_NB_SHIFT
			| control->memory_width << FMC_SDCR_MWID_SHIFT
			| control->no_rows << FMC_SDCR_NR_SHIFT
			| control->no_columns << FMC_SDCR_NC_SHIFT,
			&regs->sdcr2);

	writel(timing->trcd << FMC_SDTR_TRCD_SHIFT
		| timing->trp << FMC_SDTR_TRP_SHIFT
		| timing->twr << FMC_SDTR_TWR_SHIFT
		| timing->trc << FMC_SDTR_TRC_SHIFT
		| timing->tras << FMC_SDTR_TRAS_SHIFT
		| timing->txsr << FMC_SDTR_TXSR_SHIFT
		| timing->tmrd << FMC_SDTR_TMRD_SHIFT,
		&regs->sdtr1);

	if (target_bank == SDRAM_BANK2)
		writel(timing->trcd << FMC_SDTR_TRCD_SHIFT
			| timing->trp << FMC_SDTR_TRP_SHIFT
			| timing->twr << FMC_SDTR_TWR_SHIFT
			| timing->trc << FMC_SDTR_TRC_SHIFT
			| timing->tras << FMC_SDTR_TRAS_SHIFT
			| timing->txsr << FMC_SDTR_TXSR_SHIFT
			| timing->tmrd << FMC_SDTR_TMRD_SHIFT,
			&regs->sdtr2);

	if (target_bank == SDRAM_BANK1)
		ctb = FMC_SDCMR_BANK_1;
	else
		ctb = FMC_SDCMR_BANK_2;

	writel(ctb | FMC_SDCMR_MODE_START_CLOCK, &regs->sdcmr);
	udelay(200);	/* 200 us delay, page 10, "Power-Up" */
	FMC_BUSY_WAIT(regs);

	writel(ctb | FMC_SDCMR_MODE_PRECHARGE, &regs->sdcmr);
	udelay(100);
	FMC_BUSY_WAIT(regs);

	writel((ctb | FMC_SDCMR_MODE_AUTOREFRESH | 7 << FMC_SDCMR_NRFS_SHIFT),
	       &regs->sdcmr);
	udelay(100);
	FMC_BUSY_WAIT(regs);

	writel(ctb | (SDRAM_MODE_BL << SDRAM_MODE_BL_SHIFT
	       | control->cas_latency << SDRAM_MODE_CAS_SHIFT)
	       << FMC_SDCMR_MODE_REGISTER_SHIFT | FMC_SDCMR_MODE_WRITE_MODE,
	       &regs->sdcmr);
	udelay(100);
	FMC_BUSY_WAIT(regs);

	writel(ctb | FMC_SDCMR_MODE_NORMAL, &regs->sdcmr);
	FMC_BUSY_WAIT(regs);

	/* Refresh timer */
	writel(ref_count << 1, &regs->sdrtr);

	return 0;
}

int stm32f_init_sdram(const struct stm32_bank_params *params)
{
	return stm32_init_fmc(IOMEM(STM32F_FMC_BASE), params);
}

int stm32h_init_sdram(const struct stm32_bank_params *params)
{
	struct stm32_fmc_regs __iomem *regs = IOMEM(STM32H_FMC_BASE);
	int ret;

	/* disable the FMC controller */
	clrbits_le32(&regs->bcr1, FMC_BCR1_FMCEN);
	ret = stm32_init_fmc(regs, params);
	setbits_le32(&regs->bcr1, FMC_BCR1_FMCEN);

	return ret;
}

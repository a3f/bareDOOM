/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef STM32_FMC_H_
#define STM32_FMC_H_

#include <linux/types.h>
#include <soc/stm32/gpio.h>

struct stm32_sdram_control {
	u8 no_columns;
#define		NO_COL_8	0x0
#define		NO_COL_9	0x1
#define		NO_COL_10	0x2
#define		NO_COL_11	0x3
	u8 no_rows;
#define		NO_ROW_11	0x0
#define		NO_ROW_12	0x1
#define		NO_ROW_13	0x2
	u8 memory_width;
#define		MWIDTH_8	0x0
#define		MWIDTH_16	0x1
#define		MWIDTH_32	0x2
	u8 no_banks;
#define		BANKS_2		0x0
#define		BANKS_4		0x1
	u8 cas_latency;
#define		CAS_1		0x1
#define		CAS_2		0x2
#define		CAS_3		0x3
	u8 sdclk;
#define		SDCLK_DIS	0x0
#define		SDCLK_2		0x2
#define		SDCLK_3		0x3
	u8 rd_burst;
#define		RD_BURST_EN	0x1
#define		RD_BURST_DIS	0x0
	u8 rd_pipe_delay;
#define		RD_PIPE_DL_0	0x0
#define		RD_PIPE_DL_1	0x1
#define		RD_PIPE_DL_2	0x2
};

struct stm32_sdram_timing { /* Each Timing = value +1 cycles */
	u8 tmrd;
#define		TMRD_1		(1 - 1)
#define		TMRD_2		(2 - 1)
#define		TMRD_3		(3 - 1)
	u8 txsr;
#define		TXSR_1		(1 - 1)
#define		TXSR_6		(6 - 1)
#define		TXSR_7		(7 - 1)
#define		TXSR_8		(8 - 1)
	u8 tras;
#define		TRAS_1		(1 - 1)
#define		TRAS_4		(4 - 1)
#define		TRAS_6		(6 - 1)
	u8 trc;
#define		TRC_6		(6 - 1)
#define		TRC_7		(7 - 1)
	u8 trp;
#define		TRP_2		(2 - 1)
	u8 twr;
#define		TWR_1		(1 - 1)
#define		TWR_2		(2 - 1)
	u8 trcd;
#define		TRCD_1		(1 - 1)
#define		TRCD_2		(2 - 1)
};
enum stm32_fmc_bank {
	SDRAM_BANK1,
	SDRAM_BANK2,
	MAX_SDRAM_BANK,
};

struct stm32_bank_params {
	struct stm32_sdram_control sdram_control;
	struct stm32_sdram_timing sdram_timing;
	u32 sdram_ref_count;
	enum stm32_fmc_bank target_bank;
};

int stm32f_init_sdram(const struct stm32_bank_params *params);
int stm32h_init_sdram(const struct stm32_bank_params *params);

#define MEM_MODE_MASK	GENMASK(2, 0)
#define SWP_FMC_OFFSET 10
#define SWP_FMC_MASK	GENMASK(SWP_FMC_OFFSET+1, SWP_FMC_OFFSET)

/* set fmc swapping selection */
static inline void stm32_syscfg_swp_fmc(void __iomem *syscfg_base, u32 swp_fmc)
{
	clrsetbits_le32(syscfg_base, SWP_FMC_MASK, swp_fmc << SWP_FMC_OFFSET);
}

/* set memory mapping selection */
static inline void stm32_syscfg_mem_remap(void __iomem *syscfg_base, u32 mem_remap)
{
	clrsetbits_le32(syscfg_base, MEM_MODE_MASK, mem_remap);
}

#define stm32f_syscfg_mem_remap(val) stm32_syscfg_mem_remap(IOMEM(STM32F_SYSCFG_BASE), val)
#define stm32h_syscfg_mem_remap(val) stm32_syscfg_mem_remap(IOMEM(STM32H_SYSCFG_BASE), val)
#define stm32f_syscfg_swp_fmc(val) stm32_syscfg_swp_fmc(IOMEM(STM32F_SYSCFG_BASE), val)
#define stm32h_syscfg_swp_fmc(val) stm32_syscfg_swp_fmc(IOMEM(STM32H_SYSCFG_BASE), val)


static inline void stm32_pinconf_sdram(void __iomem *gpio, u32 pins)
{
	int i;

	for (i = 0; i < 32; i++) {
		if (!(pins & BIT(i)))
			continue;
		__stm32_pmx_set_mode(gpio, i, STM32_PINMODE_AF, 12);
		__stm32_pmx_set_speed(gpio, i, 2);
	}
}

#endif

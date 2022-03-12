// SPDX-License-Identifier: GPL-2.0+
#include <common.h>
#include <asm/barebox-arm.h>
#include <asm/mpu.h>
#include <debug_ll.h>
#include <mach/stm32.h>
#include <mach/stm32-rcc.h>
#include <mach/stm32-fmc.h>
#include <asm/cache-armv7m.h>
#include <soc/stm32/gpio.h>

static const struct stm32_bank_params stm32f429_disco_sdram_params = {
	.sdram_control = {
		.no_columns	= NO_COL_8,	.no_rows	= NO_ROW_12,
		.memory_width	= MWIDTH_16,	.no_banks	= BANKS_4,
		.cas_latency	= CAS_3,	.sdclk		= SDCLK_2,
		.rd_burst	= RD_BURST_EN,	.rd_pipe_delay	= RD_PIPE_DL_0,
	},
	.sdram_timing = {
		.tmrd = TMRD_3,	.txsr = TXSR_7,	.tras = TRAS_4,	.trc = TRC_6,
		.trp = TRP_2,	.twr = TWR_2,	.trcd = TRCD_2,
	},
	.sdram_ref_count = 1386,
	.target_bank = SDRAM_BANK2,
};

static inline void sdram_gpio_setup(void)
{
#define GPIO(ch) IOMEM(STM32_GPIO_BASE((ch) - 'A'))
	stm32_pinconf_sdram(GPIO('D'), 0xC703); /* D00-D03, D13-D15 */
	stm32_pinconf_sdram(GPIO('E'), 0xff83); /* NBL0-NBL1, D04-D12 */
	stm32_pinconf_sdram(GPIO('G'), 0x8133); /* A10-A11, BA0-BA1, SDNCLK, SDNCAS */
	stm32_pinconf_sdram(GPIO('F'), 0xf83f); /* A00-A09, SNDRAS */
	stm32_pinconf_sdram(GPIO('C'), BIT(0)); /* SDNWE */
	stm32_pinconf_sdram(GPIO('B'), BIT(5) | BIT(6)); /* SDCKE1, SDNE1 */
}

static void setup_dram(void)
{
	v7m_mmu_cache_invalidate();
	v7m_mmu_cache_on();
	mpu_early_init();

	stm32f42x_pll_init(8);

	rcc_gpio_clock_enable();
	rcc_fmc_clock_enable();
	sdram_gpio_setup();

	stm32f_init_sdram(&stm32f429_disco_sdram_params);
	/* swap banks, because we can't execute at default 0xD0000000 */
	stm32f_syscfg_swp_fmc(1);
}

ENTRY_FUNCTION_WITHSTACK(start_stm32f429_disco, 0x10010000, arg0, arg1, arg2)
{
	extern char __dtb_z_stm32f429_disco_start[];

	if (get_pc() < STM32F_SDRAM_BANK2_BASE)
		setup_dram();

	barebox_arm_entry(STM32F_SDRAM_BANK2_BASE, SZ_8M,
			  __dtb_z_stm32f429_disco_start);
}

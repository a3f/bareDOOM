// SPDX-License-Identifier: GPL-2.0+
#include <common.h>
#include <asm/barebox-arm.h>
#include <asm/mpu.h>
#include <debug_ll.h>
#include <mach/early_udelay.h>
#include <mach/stm32.h>
#include <mach/stm32-rcc.h>
#include <mach/stm32-fmc.h>
#include <asm/cache-armv7m.h>
#include <soc/stm32/gpio.h>

/* Set the default clock frequencies after reset. */
#if 0
extern u32 rcc_apb2_frequency;
#else
#define rcc_apb2_frequency 90000000
#endif

static inline void clock_setup(void)
{
	/* Enable GPIOG clock for LED & USARTs. */
	barebox_rcc_periph_clock_enable(RCC_GPIOA);
	barebox_rcc_periph_clock_enable(RCC_GPIOB);
	barebox_rcc_periph_clock_enable(RCC_GPIOC);
	barebox_rcc_periph_clock_enable(RCC_GPIOD);
	barebox_rcc_periph_clock_enable(RCC_GPIOE);
	barebox_rcc_periph_clock_enable(RCC_GPIOF);
	barebox_rcc_periph_clock_enable(RCC_GPIOG);

	/* Enable clocks for USART1. */
	barebox_rcc_periph_clock_enable(RCC_USART1);

	barebox_rcc_periph_clock_enable(RCC_TIM5);
}

#define USART1				USART1_BASE

#define USART_BRR(usart_base)		MMIO32((usart_base) + 0x08)
#define USART_CR1(usart_base)		MMIO32((usart_base) + 0x0c)
#define USART_CR2(usart_base)		MMIO32((usart_base) + 0x10)
#define USART_CR3(usart_base)		MMIO32((usart_base) + 0x14)

#define USART_CR2_STOPBITS_1		(0x00 << 12)     /* 1 stop bit */
#define USART_CR2_STOPBITS_MASK		(0x03 << 12)

#define USART_STOPBITS_1		USART_CR2_STOPBITS_1   /* 1 stop bit */
#define USART_SR_TXE			(1 << 7)
#define USART_CR1_RE			(1 << 2)
#define USART_CR1_TE			(1 << 3)
#define USART_CR1_M			(1 << 12)
#define USART_CR1_UE			(1 << 13)
#define USART_MODE_TX		        USART_CR1_TE
#define USART_PARITY_NONE		0x00
#define USART_FLOWCONTROL_NONE	        0x00
#define USART_MODE_MASK		        (USART_CR1_RE | USART_CR1_TE)

static inline void usart_set_baudrate(u32 usart, u32 baud)
{
	USART_BRR(usart) = (rcc_apb2_frequency + baud / 2) / baud;
}

static inline void usart_set_stopbits(u32 usart, u32 stopbits)
{
	u32 reg32;

	reg32 = USART_CR2(usart);
	reg32 = (reg32 & ~USART_CR2_STOPBITS_MASK) | stopbits;
	USART_CR2(usart) = reg32;
}

static inline void usart_set_mode(u32 usart, u32 mode)
{
	u32 reg32;

	reg32 = USART_CR1(usart);
	reg32 = (reg32 & ~USART_MODE_MASK) | mode;
	USART_CR1(usart) = reg32;
}

#define USART_CR3_CTSE			(1 << 9)
#define USART_CR3_RTSE			(1 << 8)

#define USART_FLOWCONTROL_MASK	        (USART_CR3_RTSE | USART_CR3_CTSE)

static inline void usart_set_flow_control(u32 usart, u32 flowcontrol)
{
	u32 reg32;

	reg32 = USART_CR3(usart);
	reg32 = (reg32 & ~USART_FLOWCONTROL_MASK) | flowcontrol;
	USART_CR3(usart) = reg32;
}

static inline void usart_enable(u32 usart)
{
	USART_CR1(usart) |= USART_CR1_UE;
}

#define USART_CR1_PS			(1 << 9)
#define USART_CR1_PCE			(1 << 10)
#define USART_PARITY_MASK		(USART_CR1_PS | USART_CR1_PCE)

static inline void usart_set_parity(u32 usart, u32 parity)
{
	u32 reg32;

	reg32 = USART_CR1(usart);
	reg32 = (reg32 & ~USART_PARITY_MASK) | parity;
	USART_CR1(usart) = reg32;
}

#define GPIOA				IOMEM(STM32_GPIO_BASE(0))
#define GPIOB				IOMEM(STM32_GPIO_BASE(1))
#define GPIOC				IOMEM(STM32_GPIO_BASE(2))
#define GPIOD				IOMEM(STM32_GPIO_BASE(3))
#define GPIOE				IOMEM(STM32_GPIO_BASE(4))
#define GPIOF				IOMEM(STM32_GPIO_BASE(5))
#define GPIOG				IOMEM(STM32_GPIO_BASE(6))

static inline void usart_setup(void)
{

	/* Enable GPIOG clock for LED & USARTs. */
	barebox_rcc_periph_clock_enable(RCC_GPIOA);
	barebox_rcc_periph_clock_enable(RCC_GPIOG);

	/* Setup GPIO pin GPIO13 on GPIO port G for LED. */
	__stm32_pmx_set_bias(GPIOG, 13, STM32_PIN_NO_BIAS);

	/* Enable clocks for USART1. */
	barebox_rcc_periph_clock_enable(RCC_USART1);

	/* Setup GPIO pins for USART1 transmit. */
	__stm32_pmx_set_bias(GPIOA,  9, STM32_PIN_NO_BIAS);
	__stm32_pmx_set_bias(GPIOA, 10, STM32_PIN_PULL_UP);

	/* Setup USART1 TX and RX pins as alternate function. */
	__stm32_pmx_set_mode(GPIOA,  9, STM32_PINMODE_AF, 7);
	__stm32_pmx_set_mode(GPIOA, 10, STM32_PINMODE_AF, 7);

	/* Setup USART2 parameters. */
	usart_set_baudrate(USART1, 115200);
	USART_CR1(USART1) &= ~USART_CR1_M; /* 8 data bits */
	usart_set_stopbits(USART1, USART_STOPBITS_1);
	usart_set_mode(USART1, USART_MODE_TX);
	usart_set_parity(USART1, USART_PARITY_NONE);
	usart_set_flow_control(USART1, USART_FLOWCONTROL_NONE);

	/* Finally enable the USART. */
	usart_enable(USART1);
}

static inline void gpio_setup(void)
{
	stm32_pinconf_sdram(GPIOD, 0xC703); /* D00-D03, D13-D15 */
	stm32_pinconf_sdram(GPIOE, 0xff83); /* NBL0-NBL1, D04-D12 */
	stm32_pinconf_sdram(GPIOG, 0x8133); /* A10-A11, BA0-BA1, SDNCLK, SDNCAS */
	stm32_pinconf_sdram(GPIOF, 0xf83f); /* A00-A09, SNDRAS */
	stm32_pinconf_sdram(GPIOC, BIT(0)); /* SDNWE */
	stm32_pinconf_sdram(GPIOB, BIT(5) | BIT(6)); /* SDCKE1, SDNE1 */
}

static const struct stm32_bank_params stm32f429_disco_sdram_params = {
	.sdram_control = {
		.no_columns = NO_COL_8,
		.no_rows = NO_ROW_12,
		.memory_width = MWIDTH_16,
		.no_banks = BANKS_4,
		.cas_latency = CAS_3,
		.sdclk = SDCLK_2,
		.rd_burst = RD_BURST_EN,
		.rd_pipe_delay = RD_PIPE_DL_0,
	},
	.sdram_timing = {
		.tmrd = TMRD_3,
		.txsr = TXSR_7,
		.tras = TRAS_4,
		.trc = TRC_6,
		.trp = TRP_2,
		.twr = TWR_2,
		.trcd = TRCD_2,
	},
	.sdram_ref_count = 1386,
	.target_bank = SDRAM_BANK2,
};

extern char __dtb_z_stm32f429_disco_start[];

extern void clock_setup(void);

static inline void stm32_cpu_lowlevel_init(void)
{
	v7m_mmu_cache_invalidate();
	v7m_mmu_cache_on();
	mpu_early_init();
}

static void noinline continue_stm32f429_disco2(void)
{
	void *fdt;

	stm32_cpu_lowlevel_init();

	stm32f42x_pll_init(8);
	clock_setup();
	usart_setup();
	gpio_setup();
	barebox_rcc_periph_clock_enable(RCC_FMC);

	stm32f_init_sdram(&stm32f429_disco_sdram_params);
	/* swap banks, because we can't execute at default 0xD0000000 */
	stm32f_syscfg_swp_fmc(1);

	fdt = __dtb_z_stm32f429_disco_start + get_runtime_offset();

	barebox_arm_entry(0x90000000, SZ_8M, fdt);
}

void continue_stm32f429_disco(u32 arg0, u32 arg1, u32 arg2)
{
	relocate_to_adr(STM32F_SRAM_BASE);
	setup_c();

	continue_stm32f429_disco2();
}

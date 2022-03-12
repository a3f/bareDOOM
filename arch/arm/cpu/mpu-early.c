// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (C) 2017, STMicroelectronics - All Rights Reserved
 * Author(s): Vikas Manocha, <vikas.manocha@st.com> for STMicroelectronics.
 */

#include <linux/kernel.h>
#include <linux/bitops.h>
#include <asm/mpu.h>
#include <asm/system.h>
#include <asm/armv7m.h>
#include <asm/io.h>

#define V7M_MPU_CTRL_ENABLE		BIT(0)
#define V7M_MPU_CTRL_DISABLE		(0 << 0)
#define V7M_MPU_CTRL_HFNMIENA		BIT(1)
#define V7M_MPU_CTRL_PRIVDEFENA		BIT(2)
#define VALID_REGION			BIT(4)

void disable_mpu(void)
{
	writel(0, &V7M_MPU->ctrl);
}

void enable_mpu(void)
{
	writel(V7M_MPU_CTRL_ENABLE | V7M_MPU_CTRL_PRIVDEFENA, &V7M_MPU->ctrl);

	/* Make sure new mpu config is effective for next memory access */
	dsb();
	isb();	/* Make sure instruction stream sees it */
}

void mpu_config(struct mpu_region_config *reg_config)
{
	uint32_t attr;

	attr = get_attr_encoding(reg_config->mr_attr);

	writel(reg_config->start_addr | VALID_REGION | reg_config->region_no,
	       &V7M_MPU->rbar);

	writel(reg_config->xn << XN_SHIFT | reg_config->ap << AP_SHIFT | attr
		| reg_config->reg_size << REGION_SIZE_SHIFT | ENABLE_REGION, &V7M_MPU->rasr);
}

void mpu_early_init(void)
{
	int i;

	struct mpu_region_config stm32_region_config[] = {
		/*
		 * Make SDRAM area cacheable & executable.
		 */
		{ 0x00000000, REGION_0, XN_DIS, PRIV_RW_USR_RW,
		O_I_WB_RD_WR_ALLOC, REGION_512MB },

		{ 0x90000000, REGION_1, XN_DIS, PRIV_RW_USR_RW,
		SHARED_WRITE_BUFFERED, REGION_256MB },

	};

	disable_mpu();
	for (i = 0; i < ARRAY_SIZE(stm32_region_config); i++)
		mpu_config(&stm32_region_config[i]);
	enable_mpu();
}

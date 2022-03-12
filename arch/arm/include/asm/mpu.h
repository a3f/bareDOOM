/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Copyright (C) 2017, STMicroelectronics - All Rights Reserved
 * Author(s): Vikas Manocha, <vikas.manocha@st.com> for STMicroelectronics.
 */

#ifndef _ASM_MPU_H
#define _ASM_MPU_H

#define REGION_SIZE_SHIFT	1
#define ENABLE_REGION		BIT(0)
#define DISABLE_REGION		0

#ifndef __ASSEMBLY__
#include <linux/bitops.h>
#include <linux/types.h>

enum region_number {
	REGION_0 = 0,
	REGION_1,
	REGION_2,
	REGION_3,
	REGION_4,
	REGION_5,
	REGION_6,
	REGION_7,
};

enum ap {
	NO_ACCESS = 0,
	PRIV_RW_USR_NO,
	PRIV_RW_USR_RO,
	PRIV_RW_USR_RW,
	UNPREDICTABLE,
	PRIV_RO_USR_NO,
	PRIV_RO_USR_RO,
};

enum mr_attr {
	STRONG_ORDER = 0,
	SHARED_WRITE_BUFFERED,
	O_I_WT_NO_WR_ALLOC,
	O_I_WB_NO_WR_ALLOC,
	O_I_NON_CACHEABLE,
	O_I_WB_RD_WR_ALLOC,
	DEVICE_NON_SHARED,
};
enum size {
	REGION_8MB = 22,
	REGION_16MB,
	REGION_32MB,
	REGION_64MB,
	REGION_128MB,
	REGION_256MB,
	REGION_512MB,
	REGION_1GB,
	REGION_2GB,
	REGION_4GB,
};

enum xn {
	XN_DIS = 0,
	XN_EN,
};

struct mpu_region_config {
	u32 start_addr;
	enum region_number region_no;
	enum xn xn;
	enum ap ap;
	enum mr_attr mr_attr;
	enum size reg_size;
};

#ifdef CONFIG_MPU

void disable_mpu(void);
void enable_mpu(void);
int mpu_enabled(void);
void mpu_config(struct mpu_region_config *reg_config);
void setup_mpu_regions(struct mpu_region_config *rgns, u32 num_rgns);

void disable_mpu(void);
void enable_mpu(void);
int mpu_enabled(void);
void mpu_config(struct mpu_region_config *reg_config);
void setup_mpu_regions(struct mpu_region_config *rgns, u32 num_rgns);

void mpu_early_init(void);

#else

static inline void disable_mpu(void)
{
}
static inline void enable_mpu(void)
{
}
static inline int mpu_enabled(void)
{
	return 0;
}
static inline void mpu_config(struct mpu_region_config *reg_config)
{
}
static inline void setup_mpu_regions(struct mpu_region_config *rgns, u32 num_rgns)
{
}

static inline void mpu_early_init(void)
{
}

#endif

#endif

#if __LINUX_ARM_ARCH__ >= 7 && defined(CONFIG_CPU_32v7M)
#include <asm/armv7m-mpu.h>
#endif

#endif

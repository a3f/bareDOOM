/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Copyright (C) 2017, STMicroelectronics - All Rights Reserved
 * Author(s): Vikas Manocha, <vikas.manocha@st.com> for STMicroelectronics.
 */

#ifndef _ASM_ARMV7M_MPU_H
#define _ASM_ARMV7M_MPU_H

#define AP_SHIFT		24
#define XN_SHIFT		28
#define TEX_SHIFT		19
#define S_SHIFT			18
#define C_SHIFT			17
#define B_SHIFT			16

#define CACHEABLE		BIT(C_SHIFT)
#define BUFFERABLE		BIT(B_SHIFT)
#define SHAREABLE		BIT(S_SHIFT)

#ifndef __ASSEMBLY__
static inline u32 get_attr_encoding(u32 mr_attr)
{
	u32 attr;

	switch (mr_attr) {
	case STRONG_ORDER:
		attr = SHAREABLE;
		break;
	case SHARED_WRITE_BUFFERED:
		attr = BUFFERABLE;
		break;
	case O_I_WT_NO_WR_ALLOC:
		attr = CACHEABLE;
		break;
	case O_I_WB_NO_WR_ALLOC:
		attr = CACHEABLE | BUFFERABLE;
		break;
	case O_I_NON_CACHEABLE:
		attr = 1 << TEX_SHIFT;
		break;
	case O_I_WB_RD_WR_ALLOC:
		attr = (1 << TEX_SHIFT) | CACHEABLE | BUFFERABLE;
		break;
	case DEVICE_NON_SHARED:
		attr = (2 << TEX_SHIFT) | BUFFERABLE;
		break;
	default:
		attr = 0; /* strongly ordered */
		break;
	};

	return attr;
}
#endif

#endif

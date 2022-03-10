// SPDX-License-Identifier: GPL-2.0-only
// SPDX-FileCopyrightText: 2012 Sascha Hauer <s.hauer@pengutronix.de>, Pengutronix

/* esdctl.c - i.MX sdram controller functions */

#include <common.h>
#include <io.h>
#include <errno.h>
#include <linux/sizes.h>
#include <init.h>
#include <of.h>
#include <linux/err.h>
#include <linux/bitfield.h>
#include <asm/barebox-arm.h>
#include <asm/memory.h>
#include <mach/esdctl.h>
#include <mach/esdctl-v4.h>
#include <mach/imx6-mmdc.h>
#include <mach/imx1-regs.h>
#include <mach/imx21-regs.h>
#include <mach/imx25-regs.h>
#include <mach/imx27-regs.h>
#include <mach/imx31-regs.h>
#include <mach/imx35-regs.h>
#include <mach/imx51-regs.h>
#include <mach/imx53-regs.h>
#include <mach/imx6-regs.h>
#include <mach/vf610-ddrmc.h>
#include <mach/imx8m-regs.h>
#include <mach/imx7-regs.h>

struct imx_esdctl_data {
	unsigned long base0;
	unsigned long base1;
	int (*add_mem)(void *esdctlbase, struct imx_esdctl_data *);
};

static int imx_esdctl_disabled;

/*
 * Boards can disable SDRAM detection if it doesn't work for them. In
 * this case arm_add_mem_device has to be called by board code.
 */
void imx_esdctl_disable(void)
{
	imx_esdctl_disabled = 1;
}

/*
 * v1 - found on i.MX1
 */
static inline unsigned long imx_v1_sdram_size(void __iomem *esdctlbase, int num)
{
	void __iomem *esdctl = esdctlbase + (num ? 4 : 0);
	u32 ctlval = readl(esdctl);
	unsigned long size;
	int rows, cols, width = 2, banks = 4;

	if (!(ctlval & ESDCTL0_SDE))
		/* SDRAM controller disabled, so no RAM here */
		return 0;

	rows = ((ctlval >> 24) & 0x3) + 11;
	cols = ((ctlval >> 20) & 0x3) + 8;

	if (ctlval & (1 << 17))
		width = 4;

	size = memory_sdram_size(cols, rows, banks, width);

	return min_t(unsigned long, size, SZ_64M);
}

/*
 * v2 - found on i.MX25, i.MX27, i.MX31 and i.MX35
 */
static inline unsigned long imx_v2_sdram_size(void __iomem *esdctlbase, int num)
{
	void __iomem *esdctl = esdctlbase + (num ? IMX_ESDCTL1 : IMX_ESDCTL0);
	u32 ctlval = readl(esdctl);
	unsigned long size;
	int rows, cols, width = 2, banks = 4;

	if (!(ctlval & ESDCTL0_SDE))
		/* SDRAM controller disabled, so no RAM here */
		return 0;

	rows = ((ctlval >> 24) & 0x7) + 11;
	cols = ((ctlval >> 20) & 0x3) + 8;

	if ((ctlval & ESDCTL0_DSIZ_MASK) == ESDCTL0_DSIZ_31_0)
		width = 4;

	size = memory_sdram_size(cols, rows, banks, width);

	return min_t(unsigned long, size, SZ_256M);
}

/*
 * v3 - found on i.MX51
 */
static inline unsigned long imx_v3_sdram_size(void __iomem *esdctlbase, int num)
{
	unsigned long size;

	size = imx_v2_sdram_size(esdctlbase, num);

	if (readl(esdctlbase + IMX_ESDMISC) & ESDMISC_DDR2_8_BANK)
		size *= 2;

	return min_t(unsigned long, size, SZ_256M);
}

/*
 * v4 - found on i.MX53
 */
static inline unsigned long imx_v4_sdram_size(void __iomem *esdctlbase, int cs)
{
	u32 ctlval = readl(esdctlbase + ESDCTL_V4_ESDCTL0);
	u32 esdmisc = readl(esdctlbase + ESDCTL_V4_ESDMISC);
	int rows, cols, width = 2, banks = 8;

	if (cs == 0 && !(ctlval & ESDCTL_V4_ESDCTLx_SDE0))
		return 0;
	if (cs == 1 && !(ctlval & ESDCTL_V4_ESDCTLx_SDE1))
		return 0;
	/* one 2GiB cs, memory is returned for cs0 only */
	if (cs == 1 && (esdmisc & ESDCTL_V4_ESDMISC_ONE_CS))
		return 0;
	rows = ((ctlval >> 24) & 0x7) + 11;

	cols = (ctlval >> 20) & 0x7;
	if (cols == 3)
		cols = 8;
	else if (cols == 4)
		cols = 12;
	else
		cols += 9;

	if (ctlval & ESDCTL_V4_ESDCTLx_DSIZ_32B)
		width = 4;

	if (esdmisc & ESDCTL_V4_ESDMISC_BANKS_4)
		banks = 4;

	return memory_sdram_size(cols, rows, banks, width);
}

/*
 * MMDC - found on i.MX6
 */

static inline u64 __imx6_mmdc_sdram_size(void __iomem *mmdcbase, int cs)
{
	u32 ctlval = readl(mmdcbase + MDCTL);
	u32 mdmisc = readl(mmdcbase + MDMISC);
	int rows, cols, width = 2, banks = 8;

	if (cs == 0 && !(ctlval & MMDCx_MDCTL_SDE0))
		return 0;
	if (cs == 1 && !(ctlval & MMDCx_MDCTL_SDE1))
		return 0;

	rows = ((ctlval >> 24) & 0x7) + 11;

	cols = (ctlval >> 20) & 0x7;
	if (cols == 3)
		cols = 8;
	else if (cols == 4)
		cols = 12;
	else
		cols += 9;

	if (ctlval & MMDCx_MDCTL_DSIZ_32B)
		width = 4;
	else if (ctlval & MMDCx_MDCTL_DSIZ_64B)
		width = 8;

	if (mdmisc & MMDCx_MDMISC_DDR_4_BANKS)
		banks = 4;

	return memory_sdram_size(cols, rows, banks, width);
}

static int add_mem(unsigned long base0, unsigned long size0,
		unsigned long base1, unsigned long size1)
{
	int ret0 = 0, ret1 = 0;

	debug("%s: cs0 base: 0x%08lx cs0 size: 0x%08lx\n", __func__, base0, size0);
	debug("%s: cs1 base: 0x%08lx cs1 size: 0x%08lx\n", __func__, base1, size1);

	if (base0 + size0 == base1 && size1 > 0) {
		/*
		 * concatenate both chip selects to a single bank
		 */
		return arm_add_mem_device("ram0", base0, size0 + size1);
	}

	if (size0)
		ret0 = arm_add_mem_device("ram0", base0, size0);

	if (size1)
		ret1 = arm_add_mem_device(size0 ? "ram1" : "ram0", base1, size1);

	return ret0 ? ret0 : ret1;
}

/*
 * On i.MX27, i.MX31 and i.MX35 the second chipselect is enabled by reset default.
 * This setting makes it impossible to detect the correct SDRAM size on
 * these SoCs. We disable the chipselect if this reset default setting is
 * found. This of course leads to incorrect SDRAM detection on boards which
 * really have this reset default as a valid setting. If you have such a
 * board drop a mail to search for a solution.
 */
#define ESDCTL1_RESET_DEFAULT 0x81120080

static inline void imx_esdctl_v2_disable_default(void __iomem *esdctlbase)
{
	u32 ctlval = readl(esdctlbase + IMX_ESDCTL1);

	if (ctlval == ESDCTL1_RESET_DEFAULT) {
		ctlval &= ~(1 << 31);
		writel(ctlval, esdctlbase + IMX_ESDCTL1);
	}
}

static int imx_esdctl_v1_add_mem(void *esdctlbase, struct imx_esdctl_data *data)
{
	return add_mem(data->base0, imx_v1_sdram_size(esdctlbase, 0),
			data->base1, imx_v1_sdram_size(esdctlbase, 1));
}

static int imx_esdctl_v2_add_mem(void *esdctlbase, struct imx_esdctl_data *data)
{
	return add_mem(data->base0, imx_v2_sdram_size(esdctlbase, 0),
			data->base1, imx_v2_sdram_size(esdctlbase, 1));
}

static int imx_esdctl_v2_bug_add_mem(void *esdctlbase, struct imx_esdctl_data *data)
{
	imx_esdctl_v2_disable_default(esdctlbase);

	return add_mem(data->base0, imx_v2_sdram_size(esdctlbase, 0),
			data->base1, imx_v2_sdram_size(esdctlbase, 1));
}

static int imx_esdctl_v3_add_mem(void *esdctlbase, struct imx_esdctl_data *data)
{
	return add_mem(data->base0, imx_v3_sdram_size(esdctlbase, 0),
			data->base1, imx_v3_sdram_size(esdctlbase, 1));
}

static int imx_esdctl_v4_add_mem(void *esdctlbase, struct imx_esdctl_data *data)
{
	return add_mem(data->base0, imx_v4_sdram_size(esdctlbase, 0),
			data->base1, imx_v4_sdram_size(esdctlbase, 1));
}

/*
 * On i.MX6 the adress space reserved for SDRAM is 0x10000000 to 0xFFFFFFFF
 * which makes the maximum supported RAM size 0xF0000000.
 */
#define IMX6_MAX_SDRAM_SIZE 0xF0000000

static inline resource_size_t imx6_mmdc_sdram_size(void __iomem *mmdcbase)
{
	/*
	 * It is possible to have a configuration in which both chip
	 * selects of the memory controller have 2GB of memory. To
	 * account for this case we need to use 64-bit arithmetic and
	 * also make sure we do not report more than
	 * IMX6_MAX_SDRAM_SIZE bytes of memory available.
	 */

	u64 size_cs0 = __imx6_mmdc_sdram_size(mmdcbase, 0);
	u64 size_cs1 = __imx6_mmdc_sdram_size(mmdcbase, 1);
	u64 total    = size_cs0 + size_cs1;

	resource_size_t size = min(total, (u64)IMX6_MAX_SDRAM_SIZE);

	return size;
}

static int imx6_mmdc_add_mem(void *mmdcbase, struct imx_esdctl_data *data)
{
	return arm_add_mem_device("ram0", data->base0,
			   imx6_mmdc_sdram_size(mmdcbase));
}

static inline resource_size_t vf610_ddrmc_sdram_size(void __iomem *ddrmc)
{
	const u32 cr01 = readl(ddrmc + DDRMC_CR(1));
	const u32 cr73 = readl(ddrmc + DDRMC_CR(73));
	const u32 cr78 = readl(ddrmc + DDRMC_CR(78));

	unsigned int rows, cols, width, banks;

	rows  = DDRMC_CR01_MAX_ROW_REG(cr01) - DDRMC_CR73_ROW_DIFF(cr73);
	cols  = DDRMC_CR01_MAX_COL_REG(cr01) - DDRMC_CR73_COL_DIFF(cr73);
	banks = 1 << (3 - DDRMC_CR73_BANK_DIFF(cr73));
	width = (cr78 & DDRMC_CR78_REDUC) ? sizeof(u8) : sizeof(u16);

	return memory_sdram_size(cols, rows, banks, width);
}

static int vf610_ddrmc_add_mem(void *mmdcbase, struct imx_esdctl_data *data)
{
	return arm_add_mem_device("ram0", data->base0,
			   vf610_ddrmc_sdram_size(mmdcbase));
}

#define DDRC_ADDRMAP(n)				(0x200 + 4 * (n))
#define DDRC_ADDRMAP6_LPDDR4_6GB_12GB_24GB	GENMASK(30, 29)
#define DDRC_ADDRMAP6_LPDDR3_6GB_12GB		BIT(31)
#define DDRC_ADDRMAP0_CS_BIT0			GENMASK(4, 0)

#define DDRC_MSTR				0x0000
#define DDRC_MSTR_LPDDR4			BIT(5)
#define DDRC_MSTR_DATA_BUS_WIDTH		GENMASK(13, 12)
#define DDRC_MSTR_ACTIVE_RANKS			GENMASK(27, 24)
#define DDRC_MSTR_DEVICE_CONFIG		GENMASK(31, 30)

#define DDRC_ADDRMAP0_CS_BIT1			GENMASK(12,  8)

#define DDRC_ADDRMAP1_BANK_B2			GENMASK(20, 16)

#define DDRC_ADDRMAP2_COL_B5			GENMASK(27, 24)
#define DDRC_ADDRMAP2_COL_B4			GENMASK(19, 16)

#define DDRC_ADDRMAP3_COL_B9			GENMASK(27, 24)
#define DDRC_ADDRMAP3_COL_B8			GENMASK(19, 16)
#define DDRC_ADDRMAP3_COL_B7			GENMASK(11,  8)
#define DDRC_ADDRMAP3_COL_B6			GENMASK( 3,  0)

#define DDRC_ADDRMAP4_COL_B10			GENMASK(3, 0)
#define DDRC_ADDRMAP4_COL_B11			GENMASK(11, 8)

#define DDRC_ADDRMAP5_ROW_B11			GENMASK(27, 24)

#define DDRC_ADDRMAP6_ROW_B15			GENMASK(27, 24)
#define DDRC_ADDRMAP6_ROW_B14			GENMASK(19, 16)
#define DDRC_ADDRMAP6_ROW_B13			GENMASK(11,  8)
#define DDRC_ADDRMAP6_ROW_B12			GENMASK( 3,  0)

#define DDRC_ADDRMAP7_ROW_B17			GENMASK(11,  8)
#define DDRC_ADDRMAP7_ROW_B16			GENMASK( 3,  0)

#define DDRC_ADDRMAP8_BG_B1			GENMASK(13,  8)
#define DDRC_ADDRMAP8_BG_B0			GENMASK(4,  0)

static unsigned int
imx_ddrc_count_bits(unsigned int bits, const u8 config[],
		     unsigned int config_num)
{
	unsigned int i;

	for (i = 0; i < config_num; i++) {
		if (config[i] == 0b1111)
			bits--;
	}

	return bits;
}

static resource_size_t
imx_ddrc_sdram_size(void __iomem *ddrc, const u32 addrmap[],
		    u8 col_max, const u8 col_b[], unsigned int col_b_num,
		    u8 row_max, const u8 row_b[], unsigned int row_b_num,
		    bool reduced_adress_space, bool is_imx8)
{
	const u32 mstr = readl(ddrc + DDRC_MSTR);
	unsigned int banks, ranks, columns, rows, active_ranks, width;
	resource_size_t size;

	banks = 2;
	ranks = 0;

	switch (FIELD_GET(DDRC_MSTR_ACTIVE_RANKS, mstr)) {
	case 0b0001:
		active_ranks = 1;
		break;
	case 0b0011:
		active_ranks = 2;
		break;
	case 0b1111:
		active_ranks = 4;
		break;
	default:
		BUG();
	}

	/* Bus width in bytes, 0 means half byte or 4-bit mode */
	if (is_imx8)
		width = (1 << FIELD_GET(DDRC_MSTR_DEVICE_CONFIG, mstr)) >> 1;
	else
		width = 4;

	switch (FIELD_GET(DDRC_MSTR_DATA_BUS_WIDTH, mstr)) {
	case 0b00:	/* Full DQ bus  */
		break;
	case 0b01:	/* Half DQ bus  */
		width >>= 1;
		break;
	case 0b10:	/* Quarter DQ bus  */
		width >>= 2;
		break;
	default:
		BUG();
	}

	if (active_ranks == 4 &&
	    FIELD_GET(DDRC_ADDRMAP0_CS_BIT1, addrmap[0]) != 0b11111)
		ranks++;

	if (active_ranks > 1  &&
	    FIELD_GET(DDRC_ADDRMAP0_CS_BIT0, addrmap[0]) != 0b11111)
		ranks++;

	if (FIELD_GET(DDRC_ADDRMAP1_BANK_B2, addrmap[1]) != 0b11111)
		banks++;

	if (addrmap[8]) {
		if (FIELD_GET(DDRC_ADDRMAP8_BG_B0, addrmap[8]) != 0b11111)
			banks++;
		if (FIELD_GET(DDRC_ADDRMAP8_BG_B1, addrmap[8]) != 0b111111)
			banks++;
	}

	columns = imx_ddrc_count_bits(col_max, col_b, col_b_num);
	rows    = imx_ddrc_count_bits(row_max, row_b, row_b_num);

	/*
	 * Special case when bus width is 0 or x4 mode,
	 * calculate the mem size and then divide the size by 2.
	 */
	if (width)
		size = memory_sdram_size(columns, rows, 1 << banks, width);
	else
		size = memory_sdram_size(columns, rows, 1 << banks, 1) >> 1;
	size <<= ranks;

	return reduced_adress_space ? size * 3 / 4 : size;
}

static resource_size_t imx8m_ddrc_sdram_size(void __iomem *ddrc)
{
	const u32 addrmap[] = {
		readl(ddrc + DDRC_ADDRMAP(0)),
		readl(ddrc + DDRC_ADDRMAP(1)),
		readl(ddrc + DDRC_ADDRMAP(2)),
		readl(ddrc + DDRC_ADDRMAP(3)),
		readl(ddrc + DDRC_ADDRMAP(4)),
		readl(ddrc + DDRC_ADDRMAP(5)),
		readl(ddrc + DDRC_ADDRMAP(6)),
		readl(ddrc + DDRC_ADDRMAP(7)),
		readl(ddrc + DDRC_ADDRMAP(8))
	};
	const u8 col_b[] = {
		/*
		 * FIXME: DDR register spreadsheet mentiones that B10
		 * and B11 are 5-bit fields instead of 4. Needs to be
		 * clarified.
		 */
		FIELD_GET(DDRC_ADDRMAP4_COL_B11, addrmap[4]),
		FIELD_GET(DDRC_ADDRMAP4_COL_B10, addrmap[4]),
		FIELD_GET(DDRC_ADDRMAP3_COL_B9,  addrmap[3]),
		FIELD_GET(DDRC_ADDRMAP3_COL_B8,  addrmap[3]),
		FIELD_GET(DDRC_ADDRMAP3_COL_B7,  addrmap[3]),
		FIELD_GET(DDRC_ADDRMAP3_COL_B6,  addrmap[3]),
		FIELD_GET(DDRC_ADDRMAP2_COL_B5,  addrmap[2]),
		FIELD_GET(DDRC_ADDRMAP2_COL_B4,  addrmap[2]),
	};
	const u8 row_b[] = {
		FIELD_GET(DDRC_ADDRMAP7_ROW_B17, addrmap[7]),
		FIELD_GET(DDRC_ADDRMAP7_ROW_B16, addrmap[7]),
		FIELD_GET(DDRC_ADDRMAP6_ROW_B15, addrmap[6]),
		FIELD_GET(DDRC_ADDRMAP6_ROW_B14, addrmap[6]),
		FIELD_GET(DDRC_ADDRMAP6_ROW_B13, addrmap[6]),
		FIELD_GET(DDRC_ADDRMAP6_ROW_B12, addrmap[6]),
		FIELD_GET(DDRC_ADDRMAP5_ROW_B11, addrmap[5]),
	};
	const bool reduced_adress_space =
		FIELD_GET(DDRC_ADDRMAP6_LPDDR4_6GB_12GB_24GB, addrmap[6]);

	return imx_ddrc_sdram_size(ddrc, addrmap,
				   12, ARRAY_AND_SIZE(col_b),
				   16, ARRAY_AND_SIZE(row_b),
				   reduced_adress_space, true);
}

static int imx8m_ddrc_add_mem(void *mmdcbase, struct imx_esdctl_data *data)
{
	return arm_add_mem_device("ram0", data->base0,
			   imx8m_ddrc_sdram_size(mmdcbase));
}

static resource_size_t imx7d_ddrc_sdram_size(void __iomem *ddrc)
{
	const u32 addrmap[] = {
		readl(ddrc + DDRC_ADDRMAP(0)),
		readl(ddrc + DDRC_ADDRMAP(1)),
		readl(ddrc + DDRC_ADDRMAP(2)),
		readl(ddrc + DDRC_ADDRMAP(3)),
		readl(ddrc + DDRC_ADDRMAP(4)),
		readl(ddrc + DDRC_ADDRMAP(5)),
		readl(ddrc + DDRC_ADDRMAP(6))
	};
	const u8 col_b[] = {
		FIELD_GET(DDRC_ADDRMAP4_COL_B10, addrmap[4]),
		FIELD_GET(DDRC_ADDRMAP3_COL_B9,  addrmap[3]),
		FIELD_GET(DDRC_ADDRMAP3_COL_B8,  addrmap[3]),
		FIELD_GET(DDRC_ADDRMAP3_COL_B7,  addrmap[3]),
		FIELD_GET(DDRC_ADDRMAP3_COL_B6,  addrmap[3]),
		FIELD_GET(DDRC_ADDRMAP2_COL_B5,  addrmap[2]),
		FIELD_GET(DDRC_ADDRMAP2_COL_B4,  addrmap[2]),
	};
	const u8 row_b[] = {
		FIELD_GET(DDRC_ADDRMAP6_ROW_B15, addrmap[6]),
		FIELD_GET(DDRC_ADDRMAP6_ROW_B14, addrmap[6]),
		FIELD_GET(DDRC_ADDRMAP6_ROW_B13, addrmap[6]),
		FIELD_GET(DDRC_ADDRMAP6_ROW_B12, addrmap[6]),
		FIELD_GET(DDRC_ADDRMAP5_ROW_B11, addrmap[5]),
	};
	const bool reduced_adress_space =
		FIELD_GET(DDRC_ADDRMAP6_LPDDR3_6GB_12GB, addrmap[6]);

	return imx_ddrc_sdram_size(ddrc, addrmap,
				   11, ARRAY_AND_SIZE(col_b),
				   15, ARRAY_AND_SIZE(row_b),
				   reduced_adress_space, false);
}

static int imx7d_ddrc_add_mem(void *mmdcbase, struct imx_esdctl_data *data)
{
	return arm_add_mem_device("ram0", data->base0,
			   imx7d_ddrc_sdram_size(mmdcbase));
}

static int imx_esdctl_probe(struct device_d *dev)
{
	struct resource *iores;
	struct imx_esdctl_data *data;
	int ret;
	void *base;

	ret = dev_get_drvdata(dev, (const void **)&data);
	if (ret)
		return ret;

	iores = dev_request_mem_resource(dev, 0);
	if (IS_ERR(iores))
		return PTR_ERR(iores);
	base = IOMEM(iores->start);

	if (imx_esdctl_disabled)
		return 0;

	return data->add_mem(base, data);
}

static __maybe_unused struct imx_esdctl_data imx1_data = {
	.base0 = MX1_CSD0_BASE_ADDR,
	.base1 = MX1_CSD1_BASE_ADDR,
	.add_mem = imx_esdctl_v1_add_mem,
};

static __maybe_unused struct imx_esdctl_data imx25_data = {
	.base0 = MX25_CSD0_BASE_ADDR,
	.base1 = MX25_CSD1_BASE_ADDR,
	.add_mem = imx_esdctl_v2_add_mem,
};

static __maybe_unused struct imx_esdctl_data imx27_data = {
	.base0 = MX27_CSD0_BASE_ADDR,
	.base1 = MX27_CSD1_BASE_ADDR,
	.add_mem = imx_esdctl_v2_bug_add_mem,
};

static __maybe_unused struct imx_esdctl_data imx31_data = {
	.base0 = MX31_CSD0_BASE_ADDR,
	.base1 = MX31_CSD1_BASE_ADDR,
	.add_mem = imx_esdctl_v2_bug_add_mem,
};

static __maybe_unused struct imx_esdctl_data imx35_data = {
	.base0 = MX35_CSD0_BASE_ADDR,
	.base1 = MX35_CSD1_BASE_ADDR,
	.add_mem = imx_esdctl_v2_bug_add_mem,
};

static __maybe_unused struct imx_esdctl_data imx51_data = {
	.base0 = MX51_CSD0_BASE_ADDR,
	.base1 = MX51_CSD1_BASE_ADDR,
	.add_mem = imx_esdctl_v3_add_mem,
};

static __maybe_unused struct imx_esdctl_data imx53_data = {
	.base0 = MX53_CSD0_BASE_ADDR,
	.base1 = MX53_CSD1_BASE_ADDR,
	.add_mem = imx_esdctl_v4_add_mem,
};

static __maybe_unused struct imx_esdctl_data imx6q_data = {
	.base0 = MX6_MMDC_PORT01_BASE_ADDR,
	.add_mem = imx6_mmdc_add_mem,
};

static __maybe_unused struct imx_esdctl_data imx6sx_data = {
	.base0 = MX6_MMDC_PORT0_BASE_ADDR,
	.add_mem = imx6_mmdc_add_mem,
};

static __maybe_unused struct imx_esdctl_data imx6ul_data = {
	.base0 = MX6_MMDC_PORT0_BASE_ADDR,
	.add_mem = imx6_mmdc_add_mem,
};

static __maybe_unused struct imx_esdctl_data vf610_data = {
	.base0 = VF610_RAM_BASE_ADDR,
	.add_mem = vf610_ddrmc_add_mem,
};

static __maybe_unused struct imx_esdctl_data imx8mq_data = {
	.base0 = MX8M_DDR_CSD1_BASE_ADDR,
	.add_mem = imx8m_ddrc_add_mem,
};

static __maybe_unused struct imx_esdctl_data imx7d_data = {
	.base0 = MX7_DDR_BASE_ADDR,
	.add_mem = imx7d_ddrc_add_mem,
};

static struct platform_device_id imx_esdctl_ids[] = {
#ifdef CONFIG_ARCH_IMX1
	{
		.name = "imx1-sdramc",
		.driver_data = (unsigned long)&imx1_data,
	},
#endif
#ifdef CONFIG_ARCH_IMX25
	{
		.name = "imx25-esdctl",
		.driver_data = (unsigned long)&imx25_data,
	},
#endif
#ifdef CONFIG_ARCH_IMX27
	{
		.name = "imx27-esdctl",
		.driver_data = (unsigned long)&imx27_data,
	},
#endif
#ifdef CONFIG_ARCH_IMX31
	{
		.name = "imx31-esdctl",
		.driver_data = (unsigned long)&imx31_data,
	},
#endif
#ifdef CONFIG_ARCH_IMX35
	{
		.name = "imx35-esdctl",
		.driver_data = (unsigned long)&imx35_data,
	},
#endif
#ifdef CONFIG_ARCH_IMX51
	{
		.name = "imx51-esdctl",
		.driver_data = (unsigned long)&imx51_data,
	},
#endif
#ifdef CONFIG_ARCH_IMX53
	{
		.name = "imx53-esdctl",
		.driver_data = (unsigned long)&imx53_data,
	},
#endif
	{
		/* sentinel */
	},
};

static __maybe_unused struct of_device_id imx_esdctl_dt_ids[] = {
	{
		.compatible = "fsl,imx6sl-mmdc",
		.data = &imx6ul_data
	}, {
		.compatible = "fsl,imx6ul-mmdc",
		.data = &imx6ul_data
	}, {
		.compatible = "fsl,imx6sx-mmdc",
		.data = &imx6sx_data
	}, {
		.compatible = "fsl,imx6q-mmdc",
		.data = &imx6q_data
	}, {
		.compatible = "fsl,vf610-ddrmc",
		.data = &vf610_data
	}, {
		.compatible = "fsl,imx8mm-ddrc",
		.data = &imx8mq_data
	}, {
		.compatible = "fsl,imx8mn-ddrc",
		.data = &imx8mq_data
	}, {
		.compatible = "fsl,imx8mq-ddrc",
		.data = &imx8mq_data
	}, {
		.compatible = "fsl,imx7d-ddrc",
		.data = &imx7d_data
	}, {
		/* sentinel */
	}
};

static struct driver_d imx_esdctl_driver = {
	.name   = "imx-esdctl",
	.probe  = imx_esdctl_probe,
	.id_table = imx_esdctl_ids,
	.of_compatible = DRV_OF_COMPAT(imx_esdctl_dt_ids),
};

static int imx_esdctl_init(void)
{
	int ret;

	ret = platform_driver_register(&imx_esdctl_driver);
	if (ret)
		return ret;

	return of_devices_ensure_probed_by_dev_id(imx_esdctl_dt_ids);
}
mem_initcall(imx_esdctl_init);

/*
 * The i.MX SoCs usually have two SDRAM chipselects. The following
 * SoC specific functions return:
 *
 * - cs0 disabled, cs1 disabled: 0
 * - cs0 enabled, cs1 disabled: SDRAM size for cs0
 * - cs0 disabled, c1 enabled: 0 (currently assumed that no hardware does this)
 * - cs0 enabled, cs1 enabled: The largest continuous region, that is, cs0 + cs1
 *                             if cs0 is taking the whole address space.
 */
static void
upper_or_coalesced_range(unsigned long base0, unsigned long size0,
                         unsigned long base1, unsigned long size1,
                         unsigned long *res_base, unsigned long *res_size)
{
	/* if we have an upper range, use it */
	if (size1) {
		*res_base = base1;
		*res_size = size1;
	} else {
		*res_base = base0;
		*res_size = size0;
	}

	/*
	 * if there is no hole between the two ranges, coalesce into a
	 * single big one
	 */
	if ((base0 + size0) == base1) {
		*res_base = base0;
		*res_size = size0 + size1;
	}
}

void __noreturn imx1_barebox_entry(void *boarddata)
{
	unsigned long base, size;

	upper_or_coalesced_range(MX1_CSD0_BASE_ADDR,
			imx_v1_sdram_size(IOMEM(MX1_SDRAMC_BASE_ADDR), 0),
			MX1_CSD1_BASE_ADDR,
			imx_v1_sdram_size(IOMEM(MX1_SDRAMC_BASE_ADDR), 1),
			&base, &size);

	barebox_arm_entry(base, size, boarddata);
}

void __noreturn imx25_barebox_entry(void *boarddata)
{
	unsigned long base, size;

	upper_or_coalesced_range(MX25_CSD0_BASE_ADDR,
			imx_v2_sdram_size(IOMEM(MX25_ESDCTL_BASE_ADDR), 0),
			MX25_CSD1_BASE_ADDR,
			imx_v2_sdram_size(IOMEM(MX25_ESDCTL_BASE_ADDR), 1),
			&base, &size);

	barebox_arm_entry(base, size, boarddata);
}

void __noreturn imx27_barebox_entry(void *boarddata)
{
	unsigned long base, size;

	imx_esdctl_v2_disable_default(IOMEM(MX27_ESDCTL_BASE_ADDR));

	upper_or_coalesced_range(MX27_CSD0_BASE_ADDR,
			imx_v2_sdram_size(IOMEM(MX27_ESDCTL_BASE_ADDR), 0),
			MX27_CSD1_BASE_ADDR,
			imx_v2_sdram_size(IOMEM(MX27_ESDCTL_BASE_ADDR), 1),
			&base, &size);

	barebox_arm_entry(base, size, boarddata);
}

void __noreturn imx31_barebox_entry(void *boarddata)
{
	unsigned long base, size;

	imx_esdctl_v2_disable_default(IOMEM(MX31_ESDCTL_BASE_ADDR));

	upper_or_coalesced_range(MX31_CSD0_BASE_ADDR,
			imx_v2_sdram_size(IOMEM(MX31_ESDCTL_BASE_ADDR), 0),
			MX31_CSD1_BASE_ADDR,
			imx_v2_sdram_size(IOMEM(MX31_ESDCTL_BASE_ADDR), 1),
			&base, &size);

	barebox_arm_entry(base, size, boarddata);
}

void __noreturn imx35_barebox_entry(void *boarddata)
{
	unsigned long base, size;

	imx_esdctl_v2_disable_default(IOMEM(MX35_ESDCTL_BASE_ADDR));

	upper_or_coalesced_range(MX35_CSD0_BASE_ADDR,
			imx_v2_sdram_size(IOMEM(MX35_ESDCTL_BASE_ADDR), 0),
			MX35_CSD1_BASE_ADDR,
			imx_v2_sdram_size(IOMEM(MX35_ESDCTL_BASE_ADDR), 1),
			&base, &size);

	barebox_arm_entry(base, size, boarddata);
}

void __noreturn imx51_barebox_entry(void *boarddata)
{
	unsigned long base, size;

	upper_or_coalesced_range(MX51_CSD0_BASE_ADDR,
			imx_v3_sdram_size(IOMEM(MX51_ESDCTL_BASE_ADDR), 0),
			MX51_CSD1_BASE_ADDR,
			imx_v3_sdram_size(IOMEM(MX51_ESDCTL_BASE_ADDR), 1),
			&base, &size);

	barebox_arm_entry(base, size, boarddata);
}

void __noreturn imx53_barebox_entry(void *boarddata)
{
	unsigned long base, size;

	upper_or_coalesced_range(MX53_CSD0_BASE_ADDR,
			imx_v4_sdram_size(IOMEM(MX53_ESDCTL_BASE_ADDR), 0),
			MX53_CSD1_BASE_ADDR,
			imx_v4_sdram_size(IOMEM(MX53_ESDCTL_BASE_ADDR), 1),
			&base, &size);

	barebox_arm_entry(base, size, boarddata);
}

static void __noreturn
imx6_barebox_entry(unsigned long membase, void *boarddata)
{
	barebox_arm_entry(membase,
			  imx6_mmdc_sdram_size(IOMEM(MX6_MMDC_P0_BASE_ADDR)),
			  boarddata);
}

void __noreturn imx6q_barebox_entry(void *boarddata)
{
	imx6_barebox_entry(MX6_MMDC_PORT01_BASE_ADDR, boarddata);
}

void __noreturn imx6ul_barebox_entry(void *boarddata)
{
	imx6_barebox_entry(MX6_MMDC_PORT0_BASE_ADDR, boarddata);
}

void __noreturn vf610_barebox_entry(void *boarddata)
{
	barebox_arm_entry(VF610_RAM_BASE_ADDR,
			  vf610_ddrmc_sdram_size(IOMEM(VF610_DDR_BASE_ADDR)),
			  boarddata);
}

static void __noreturn imx8m_barebox_entry(void *boarddata)
{
	resource_size_t size;

	size = imx8m_ddrc_sdram_size(IOMEM(MX8M_DDRC_CTL_BASE_ADDR));
	/*
	 * We artificially limit detected memory size to force malloc
	 * pool placement to be within 4GiB address space, so as to
	 * make it accessible to 32-bit limited DMA.
	 *
	 * This limitation affects only early boot code and malloc
	 * pool placement. The rest of the system should be able to
	 * detect and utilize full amount of memory.
	 */
	size = min_t(resource_size_t, SZ_4G - MX8M_DDR_CSD1_BASE_ADDR, size);
	barebox_arm_entry(MX8M_DDR_CSD1_BASE_ADDR, size, boarddata);
}

void __noreturn imx8mm_barebox_entry(void *boarddata)
{
	imx8m_barebox_entry(boarddata);
}

void __noreturn imx8mn_barebox_entry(void *boarddata)
{
	imx8m_barebox_entry(boarddata);
}

void __noreturn imx8mp_barebox_entry(void *boarddata)
{
	imx8m_barebox_entry(boarddata);
}

void __noreturn imx8mq_barebox_entry(void *boarddata)
{
	imx8m_barebox_entry(boarddata);
}

void __noreturn imx7d_barebox_entry(void *boarddata)
{
	barebox_arm_entry(MX7_DDR_BASE_ADDR,
			  imx7d_ddrc_sdram_size(IOMEM(MX7_DDRC_BASE_ADDR)),
			  boarddata);
}



// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright 2011 Freescale Semiconductor, Inc.
 * Copyright 2011 Linaro Ltd.
 */

#include <common.h>
#include <init.h>
#include <driver.h>
#include <linux/clk.h>
#include <io.h>
#include <of.h>
#include <linux/clkdev.h>
#include <linux/err.h>
#include <mach/imx6-regs.h>
#include <mach/revision.h>
#include <mach/imx6.h>
#include <dt-bindings/clock/imx6qdl-clock.h>

#include "clk.h"

#define CCGR0				0x68
#define CCGR1				0x6c
#define CCGR2				0x70
#define CCGR3				0x74
#define CCGR4				0x78
#define CCGR5				0x7c
#define CCGR6				0x80
#define CCGR7				0x84

#define CLPCR				0x54
#define BP_CLPCR_LPM			0
#define BM_CLPCR_LPM			(0x3 << 0)
#define BM_CLPCR_BYPASS_PMIC_READY	(0x1 << 2)
#define BM_CLPCR_ARM_CLK_DIS_ON_LPM	(0x1 << 5)
#define BM_CLPCR_SBYOS			(0x1 << 6)
#define BM_CLPCR_DIS_REF_OSC		(0x1 << 7)
#define BM_CLPCR_VSTBY			(0x1 << 8)
#define BP_CLPCR_STBY_COUNT		9
#define BM_CLPCR_STBY_COUNT		(0x3 << 9)
#define BM_CLPCR_COSC_PWRDOWN		(0x1 << 11)
#define BM_CLPCR_WB_PER_AT_LPM		(0x1 << 16)
#define BM_CLPCR_WB_CORE_AT_LPM		(0x1 << 17)
#define BM_CLPCR_BYP_MMDC_CH0_LPM_HS	(0x1 << 19)
#define BM_CLPCR_BYP_MMDC_CH1_LPM_HS	(0x1 << 21)
#define BM_CLPCR_MASK_CORE0_WFI		(0x1 << 22)
#define BM_CLPCR_MASK_CORE1_WFI		(0x1 << 23)
#define BM_CLPCR_MASK_CORE2_WFI		(0x1 << 24)
#define BM_CLPCR_MASK_CORE3_WFI		(0x1 << 25)
#define BM_CLPCR_MASK_SCU_IDLE		(0x1 << 26)
#define BM_CLPCR_MASK_L2CC_IDLE		(0x1 << 27)

static struct clk *clks[IMX6QDL_CLK_END];
static struct clk_onecell_data clk_data;

static inline int cpu_mx6_is_plus(void)
{
	return cpu_mx6_is_mx6qp() || cpu_mx6_is_mx6dp();
}

/* Audio/Video PLL post dividers don't work on i.MX6q revision 1.0 */
static inline int cpu_has_working_video_pll_post_div(void) {
	return !((cpu_mx6_is_mx6q() || cpu_mx6_is_mx6d()) &&
		 __imx6_cpu_revision() == IMX_CHIP_REV_1_0);
}

/* i.MX6 Quad/Dual/DualLite/Solo are all affected */
static inline int cpu_mx6_has_err009219(void)
{
	return cpu_mx6_is_mx6d() || cpu_mx6_is_mx6q() ||
		cpu_mx6_is_mx6dl() || cpu_mx6_is_mx6s();
}

static const char *step_sels[] = {
	"osc",
	"pll2_pfd2_396m",
};

static const char *pll1_sw_sels[] = {
	"pll1_sys",
	"step",
};

static const char *periph_pre_sels[] = {
	"pll2_bus",
	"pll2_pfd2_396m",
	"pll2_pfd0_352m",
	"pll2_198m",
};

static const char *periph_clk2_sels[] = {
	"pll3_usb_otg",
	"osc",
	"osc",
	"dummy",
};

static const char *periph2_clk2_sels[] = {
	"pll3_usb_otg",
	"pll2_bus",
};

static const char *periph_sels[] = {
	"periph_pre",
	"periph_clk2",
};

static const char *periph2_sels[] = {
	"periph2_pre",
	"periph2_clk2",
};

static const char *axi_sels[] = {
	"periph",
	"pll2_pfd2_396m",
	"periph",
	"pll3_pfd1_540m",
};

static const char *usdhc_sels[] = {
	"pll2_pfd2_396m",
	"pll2_pfd0_352m",
};

static const char *enfc_sels[]	= {
	"pll2_pfd0_352m",
	"pll2_bus",
	"pll3_usb_otg",
	"pll2_pfd2_396m",
};

static const char *enfc_sels_plus[] = {
	"pll2_pfd0_352m",
	"pll2_bus",
	"pll3_usb_otg",
	"pll2_pfd2_396m",
	"pll3_pfd3_454m",
	"dummy",
};

static const char *eim_sels[] = {
	"pll2_pfd2_396m",
	"pll3_usb_otg",
	"axi",
	"pll2_pfd0_352m",
};

static const char *eim_slow_sels[] = {
	"axi",
	"pll3_usb_otg",
	"pll2_pfd2_396m",
	"pll2_pfd0_352m",
};

static const char *vdo_axi_sels[] = {
	"axi",
	"ahb",
};

static const char *cko_sels[] = {
	"cko1",
	"cko2",
};

static const char *cko1_sels[] = {
	"pll3_usb_otg",
	"pll2_bus",
	"pll1_sys",
	"pll5_video_div",
	"video_27m",
	"axi",
	"enfc",
	"ipu1_di0",
	"ipu1_di1",
	"ipu2_di0",
	"ipu2_di1",
	"ahb",
	"ipg",
	"ipg_per",
	"ckil",
	"pll4_audio_div",
};

static const char *cko2_sels[] = {
	"mmdc_ch0_axi",
	"mmdc_ch1_axi",
	"usdhc4",
	"usdhc1",
	"gpu2d_axi",
	"dummy",
	"ecspi_root",
	"gpu3d_axi",
	"usdhc3",
	"dummy",
	"arm",
	"ipu1",
	"ipu2",
	"vdo_axi",
	"osc",
	"gpu2d_core",
	"gpu3d_core",
	"usdhc2",
	"ssi1",
	"ssi2",
	"ssi3",
	"gpu3d_shader",
	"vpu_axi",
	"can_root",
	"ldb_di0",
	"ldb_di1",
	"esai",
	"eim_slow",
	"uart_serial",
	"spdif",
	"asrc",
	"hsi_tx",
};

static const char *ipu_sels[] = {
	"mmdc_ch0_axi_podf",
	"pll2_pfd2_396m",
	"pll3_120m",
	"pll3_pfd1_540m",
};

enum ldb_di_sel { /* for use in init_ldb_clks */
	LDB_DI_SEL_PLL5_VIDEO_DIV	= 0,
	LDB_DI_SEL_PLL2_PFD0_352M	= 1,
	LDB_DI_SEL_PLL2_PFD2_396M	= 2,
	LDB_DI_SEL_MMDC_CH1_AXI		= 3,
	LDB_DI_SEL_PLL3_USB_OTG		= 4,
};

static const char *ldb_di_sels[] = {
	[LDB_DI_SEL_PLL5_VIDEO_DIV]	= "pll5_video_div",
	[LDB_DI_SEL_PLL2_PFD0_352M]	= "pll2_pfd0_352m",
	[LDB_DI_SEL_PLL2_PFD2_396M]	= "pll2_pfd2_396m",
	[LDB_DI_SEL_MMDC_CH1_AXI]	= "mmdc_ch1_axi_podf",
	[LDB_DI_SEL_PLL3_USB_OTG]	= "pll3_usb_otg",
};

static const char *ipu_di_pre_sels[] = {
	"mmdc_ch0_axi",
	"pll3_usb_otg",
	"pll5_video_div",
	"pll2_pfd0_352m",
	"pll2_pfd2_396m",
	"pll3_pfd1_540m",
};

static const char *ipu1_di0_sels[] = {
	"ipu1_di0_pre",
	"dummy",
	"dummy",
	"ldb_di0_podf",
	"ldb_di1_podf",
};

static const char *ipu1_di1_sels[] = {
	"ipu1_di1_pre",
	"dummy",
	"dummy",
	"ldb_di0_podf",
	"ldb_di1_podf",
};

static const char *ipu2_di0_sels[] = {
	"ipu2_di0_pre",
	"dummy",
	"dummy",
	"ldb_di0_podf",
	"ldb_di1_podf",
};

static const char *ipu2_di1_sels[] = {
	"ipu2_di1_pre",
	"dummy",
	"dummy",
	"ldb_di0_podf",
	"ldb_di1_podf",
};

static const char *lvds_sels[] = {
	"dummy",
	"dummy",
	"dummy",
	"dummy",
	"dummy",
	"dummy",
	"pll4_audio",
	"pll5_video",
	"pll8_mlb",
	"enet_ref",
	"pcie_ref_125m",
	"sata_ref_100m",
};

static const char *pcie_axi_sels[] = {
	"axi",
	"ahb",
};

static struct clk_div_table clk_enet_ref_table[] = {
	{ .val = 0, .div = 20, },
	{ .val = 1, .div = 10, },
	{ .val = 2, .div = 5, },
	{ .val = 3, .div = 4, },
	{ },
};

static struct clk_div_table post_div_table[] = {
	{ .val = 2, .div = 1, },
	{ .val = 1, .div = 2, },
	{ .val = 0, .div = 4, },
	{ /* sentinel */ }
};

static struct clk_div_table video_div_table[] = {
	{ .val = 0, .div = 1, },
	{ .val = 1, .div = 2, },
	{ .val = 2, .div = 1, },
	{ .val = 3, .div = 4, },
	{ /* sentinel */ }
};

static int ldb_di_sel_by_clock_id(int clock_id)
{
	switch (clock_id) {
	case IMX6QDL_CLK_PLL5_VIDEO_DIV:
		if (!cpu_has_working_video_pll_post_div())
			return -ENOENT;
		return LDB_DI_SEL_PLL5_VIDEO_DIV;
	case IMX6QDL_CLK_PLL2_PFD0_352M:
		return LDB_DI_SEL_PLL2_PFD0_352M;
	case IMX6QDL_CLK_PLL2_PFD2_396M:
		return LDB_DI_SEL_PLL2_PFD2_396M;
	case IMX6QDL_CLK_MMDC_CH1_AXI:
		return LDB_DI_SEL_MMDC_CH1_AXI;
	case IMX6QDL_CLK_PLL3_USB_OTG:
		return LDB_DI_SEL_PLL3_USB_OTG;
	default:
		return -ENOENT;
	}
}

static void of_assigned_ldb_sels(struct device_node *node,
				 enum ldb_di_sel *ldb_di0_sel,
				 enum ldb_di_sel *ldb_di1_sel)
{
	struct of_phandle_args clkspec;
	int index, rc, num_parents;
	int parent, child, sel;

	num_parents = of_count_phandle_with_args(node, "assigned-clock-parents",
						 "#clock-cells");
	for (index = 0; index < num_parents; index++) {
		rc = of_parse_phandle_with_args(node, "assigned-clock-parents",
					"#clock-cells", index, &clkspec);
		if (rc < 0) {
			/* skip empty (null) phandles */
			if (rc == -ENOENT)
				continue;
			else
				return;
		}
		if (clkspec.np != node || clkspec.args[0] >= IMX6QDL_CLK_END) {
			pr_err("ccm: parent clock %d not in ccm\n", index);
			return;
		}
		parent = clkspec.args[0];

		rc = of_parse_phandle_with_args(node, "assigned-clocks",
				"#clock-cells", index, &clkspec);
		if (rc < 0)
			return;
		if (clkspec.np != node || clkspec.args[0] >= IMX6QDL_CLK_END) {
			pr_err("ccm: child clock %d not in ccm\n", index);
			return;
		}
		child = clkspec.args[0];

		if (child != IMX6QDL_CLK_LDB_DI0_SEL &&
		    child != IMX6QDL_CLK_LDB_DI1_SEL)
			continue;

		sel = ldb_di_sel_by_clock_id(parent);
		if (sel < 0) {
			pr_err("ccm: invalid ldb_di%d parent clock: %d\n",
			       child == IMX6QDL_CLK_LDB_DI1_SEL, parent);
			continue;
		}

		if (child == IMX6QDL_CLK_LDB_DI0_SEL)
			*ldb_di0_sel = sel;
		if (child == IMX6QDL_CLK_LDB_DI1_SEL)
			*ldb_di1_sel = sel;
	}
}

#define CCM_CCDR		0x04
#define CCM_CCSR		0x0c
#define CCM_CS2CDR		0x2c

#define CCDR_MMDC_CH1_MASK		BIT(16)
#define CCSR_PLL3_SW_CLK_SEL		BIT(0)

#define CS2CDR_LDB_DI0_CLK_SEL_SHIFT	9
#define CS2CDR_LDB_DI1_CLK_SEL_SHIFT	12

static void __init imx6q_mmdc_ch1_mask_handshake(void __iomem *ccm_base)
{
	unsigned int reg;

	reg = readl(ccm_base + CCM_CCDR);
	reg |= CCDR_MMDC_CH1_MASK;
	writel(reg, ccm_base + CCM_CCDR);
}

/*
 * The only way to disable the MMDC_CH1 clock is to move it to pll3_sw_clk
 * via periph2_clk2_sel and then to disable pll3_sw_clk by selecting the
 * bypass clock source, since there is no CG bit for mmdc_ch1.
 */
static void mmdc_ch1_disable(void __iomem *ccm_base)
{
	unsigned int reg;

	clk_set_parent(clks[IMX6QDL_CLK_PERIPH2_CLK2_SEL],
		       clks[IMX6QDL_CLK_PLL3_USB_OTG]);

	/*
	 * Handshake with mmdc_ch1 module must be masked when changing
	 * periph2_clk_sel.
	 */
	clk_set_parent(clks[IMX6QDL_CLK_PERIPH2], clks[IMX6QDL_CLK_PERIPH2_CLK2]);

	/* Disable pll3_sw_clk by selecting the bypass clock source */
	reg = readl(ccm_base + CCM_CCSR);
	reg |= CCSR_PLL3_SW_CLK_SEL;
	writel(reg, ccm_base + CCM_CCSR);
}

static void mmdc_ch1_reenable(void __iomem *ccm_base)
{
	unsigned int reg;

	/* Enable pll3_sw_clk by disabling the bypass */
	reg = readl(ccm_base + CCM_CCSR);
	reg &= ~CCSR_PLL3_SW_CLK_SEL;
	writel(reg, ccm_base + CCM_CCSR);

	clk_set_parent(clks[IMX6QDL_CLK_PERIPH2], clks[IMX6QDL_CLK_PERIPH2_PRE]);
}

/*
 * We have to follow a strict procedure when changing the LDB clock source,
 * otherwise we risk introducing a glitch that can lock up the LDB divider.
 * Things to keep in mind:
 *
 * 1. The current and new parent clock inputs to the mux must be disabled.
 * 2. The default clock input for ldb_di0/1_clk_sel is mmdc_ch1_axi, which
 *    has no CG bit.
 * 3. pll2_pfd2_396m can not be gated if it is used as memory clock.
 * 4. In the RTL implementation of the LDB_DI_CLK_SEL muxes the top four
 *    options are in one mux and the PLL3 option along with three unused
 *    inputs is in a second mux. There is a third mux with two inputs used
 *    to decide between the first and second 4-port mux:
 *
 *    pll5_video_div 0 --|\
 *    pll2_pfd0_352m 1 --| |_
 *    pll2_pfd2_396m 2 --| | `-|\
 *    mmdc_ch1_axi   3 --|/    | |
 *                             | |--
 *    pll3_usb_otg   4 --|\    | |
 *                   5 --| |_,-|/
 *                   6 --| |
 *                   7 --|/
 *
 * The ldb_di0/1_clk_sel[1:0] bits control both 4-port muxes at the same time.
 * The ldb_di0/1_clk_sel[2] bit controls the 2-port mux. The code below
 * switches the parent to the bottom mux first and then manipulates the top
 * mux to ensure that no glitch will enter the divider.
 */
static void init_ldb_clks(struct device_node *np, void __iomem *ccm_base)
{
	unsigned int reg;
	enum ldb_di_sel sel[2][4];
	int i;

	reg = readl(ccm_base + CCM_CS2CDR);
	sel[0][0] = (reg >> CS2CDR_LDB_DI0_CLK_SEL_SHIFT) & 7;
	sel[1][0] = (reg >> CS2CDR_LDB_DI1_CLK_SEL_SHIFT) & 7;

	sel[0][3] = sel[0][2] = sel[0][1] = sel[0][0];
	sel[1][3] = sel[1][2] = sel[1][1] = sel[1][0];

	of_assigned_ldb_sels(np, &sel[0][3], &sel[1][3]);

	for (i = 0; i < 2; i++) {
		/* log if a glitch might have been introduced already */
		if (sel[i][0] != LDB_DI_SEL_MMDC_CH1_AXI) {
			pr_debug("ccm: ldb_di%d_sel already changed from reset value: %d\n",
				i, sel[i][0]);
		}

		if (sel[i][0] == sel[i][3])
			continue;

		/* Only switch to or from pll2_pfd2_396m if it is disabled */
		if ((sel[i][0] == LDB_DI_SEL_PLL2_PFD2_396M ||
		     sel[i][3] == LDB_DI_SEL_PLL2_PFD2_396M) &&
		    (clk_get_parent(clks[IMX6QDL_CLK_PERIPH_PRE]) ==
		     clks[IMX6QDL_CLK_PLL2_PFD2_396M])) {
			pr_err("ccm: ldb_di%d_sel: couldn't disable pll2_pfd2_396m\n",
			       i);
			sel[i][3] = sel[i][2] = sel[i][1] = sel[i][0];
			continue;
		}

		/* First switch to the bottom mux */
		sel[i][1] = sel[i][0] | 4;

		/* Then configure the top mux before switching back to it */
		sel[i][2] = sel[i][3] | 4;

		pr_debug("ccm: switching ldb_di%d_sel: %d->%d->%d->%d\n", i,
			 sel[i][0], sel[i][1], sel[i][2], sel[i][3]);
	}

	if (sel[0][0] == sel[0][3] && sel[1][0] == sel[1][3])
		return;

	mmdc_ch1_disable(ccm_base);

	for (i = 1; i < 4; i++) {
		reg = readl(ccm_base + CCM_CS2CDR);
		reg &= ~((7 << CS2CDR_LDB_DI0_CLK_SEL_SHIFT) |
			 (7 << CS2CDR_LDB_DI1_CLK_SEL_SHIFT));
		reg |= ((sel[0][i] << CS2CDR_LDB_DI0_CLK_SEL_SHIFT) |
			(sel[1][i] << CS2CDR_LDB_DI1_CLK_SEL_SHIFT));
		writel(reg, ccm_base + CCM_CS2CDR);
	}

	mmdc_ch1_reenable(ccm_base);
}

#define CCM_ANALOG_PLL_VIDEO	0xa0
#define CCM_ANALOG_PFD_480	0xf0
#define CCM_ANALOG_PFD_528	0x100

#define PLL_ENABLE		BIT(13)

#define PFD0_CLKGATE		BIT(7)
#define PFD1_CLKGATE		BIT(15)
#define PFD2_CLKGATE		BIT(23)
#define PFD3_CLKGATE		BIT(31)

static void disable_anatop_clocks(void __iomem *anatop_base)
{
	unsigned int reg;

	/* Make sure PLL2 PFDs 0-2 are gated */
	reg = readl(anatop_base + CCM_ANALOG_PFD_528);
	/* Cannot gate PFD2 if pll2_pfd2_396m is the parent of MMDC clock */
	if (clk_get_parent(clks[IMX6QDL_CLK_PERIPH_PRE]) ==
	    clks[IMX6QDL_CLK_PLL2_PFD2_396M])
		reg |= PFD0_CLKGATE | PFD1_CLKGATE;
	else
		reg |= PFD0_CLKGATE | PFD1_CLKGATE | PFD2_CLKGATE;
	writel(reg, anatop_base + CCM_ANALOG_PFD_528);

	/* Make sure PLL3 PFDs 0-3 are gated */
	reg = readl(anatop_base + CCM_ANALOG_PFD_480);
	reg |= PFD0_CLKGATE | PFD1_CLKGATE | PFD2_CLKGATE | PFD3_CLKGATE;
	writel(reg, anatop_base + CCM_ANALOG_PFD_480);

	/* Make sure PLL5 is disabled */
	reg = readl(anatop_base + CCM_ANALOG_PLL_VIDEO);
	reg &= ~PLL_ENABLE;
	writel(reg, anatop_base + CCM_ANALOG_PLL_VIDEO);
}

static void imx6_add_video_clks(void __iomem *anab, void __iomem *cb, struct device_node *ccm_np)
{
	clks[IMX6QDL_CLK_PLL5_POST_DIV] = imx_clk_divider_table("pll5_post_div", "pll5_video", anab + 0xa0, 19, 2, post_div_table);
	clks[IMX6QDL_CLK_PLL5_VIDEO_DIV] = imx_clk_divider_table("pll5_video_div", "pll5_post_div", anab + 0x170, 30, 2, video_div_table);

	clks[IMX6QDL_CLK_IPU1_SEL]         = imx_clk_mux("ipu1_sel",         cb + 0x3c, 9,  2, ipu_sels,          ARRAY_SIZE(ipu_sels));
	clks[IMX6QDL_CLK_IPU2_SEL]         = imx_clk_mux("ipu2_sel",         cb + 0x3c, 14, 2, ipu_sels,          ARRAY_SIZE(ipu_sels));

	if (cpu_mx6_has_err009219()) {
		/*
		 * The LDB_DI0/1_SEL muxes should be read-only due to a hardware
		 * bug. Set the muxes to the requested values before registering the
		 * ldb_di_sel clocks.
		 */
		init_ldb_clks(ccm_np, cb);

		clks[IMX6QDL_CLK_LDB_DI0_SEL] = imx_clk_mux_ldb("ldb_di0_sel",    cb + 0x2c, 9,  3, ldb_di_sels,      ARRAY_SIZE(ldb_di_sels));
		clks[IMX6QDL_CLK_LDB_DI1_SEL] = imx_clk_mux_ldb("ldb_di1_sel",    cb + 0x2c, 12, 3, ldb_di_sels,       ARRAY_SIZE(ldb_di_sels));
	} else {
		clks[IMX6QDL_CLK_LDB_DI0_SEL] = imx_clk_mux_p("ldb_di0_sel",    cb + 0x2c, 9,  3, ldb_di_sels,      ARRAY_SIZE(ldb_di_sels));
		clks[IMX6QDL_CLK_LDB_DI1_SEL] = imx_clk_mux_p("ldb_di1_sel",    cb + 0x2c, 12, 3, ldb_di_sels,       ARRAY_SIZE(ldb_di_sels));
	}

	clks[IMX6QDL_CLK_IPU1_DI0_PRE_SEL] = imx_clk_mux_p("ipu1_di0_pre_sel", cb + 0x34, 6,  3, ipu_di_pre_sels,   ARRAY_SIZE(ipu_di_pre_sels));
	clks[IMX6QDL_CLK_IPU1_DI1_PRE_SEL] = imx_clk_mux_p("ipu1_di1_pre_sel", cb + 0x34, 15, 3, ipu_di_pre_sels,   ARRAY_SIZE(ipu_di_pre_sels));
	clks[IMX6QDL_CLK_IPU2_DI0_PRE_SEL] = imx_clk_mux_p("ipu2_di0_pre_sel", cb + 0x38, 6,  3, ipu_di_pre_sels,   ARRAY_SIZE(ipu_di_pre_sels));
	clks[IMX6QDL_CLK_IPU2_DI1_PRE_SEL] = imx_clk_mux_p("ipu2_di1_pre_sel", cb + 0x38, 15, 3, ipu_di_pre_sels,   ARRAY_SIZE(ipu_di_pre_sels));
	clks[IMX6QDL_CLK_IPU1_DI0_SEL]     = imx_clk_mux_p("ipu1_di0_sel",     cb + 0x34, 0,  3, ipu1_di0_sels,     ARRAY_SIZE(ipu1_di0_sels));
	clks[IMX6QDL_CLK_IPU1_DI1_SEL]     = imx_clk_mux_p("ipu1_di1_sel",     cb + 0x34, 9,  3, ipu1_di1_sels,     ARRAY_SIZE(ipu1_di1_sels));
	clks[IMX6QDL_CLK_IPU2_DI0_SEL]     = imx_clk_mux_p("ipu2_di0_sel",     cb + 0x38, 0,  3, ipu2_di0_sels,     ARRAY_SIZE(ipu2_di0_sels));
	clks[IMX6QDL_CLK_IPU2_DI1_SEL]     = imx_clk_mux_p("ipu2_di1_sel",     cb + 0x38, 9,  3, ipu2_di1_sels,     ARRAY_SIZE(ipu2_di1_sels));

	clks[IMX6QDL_CLK_IPU1_PODF]        = imx_clk_divider("ipu1_podf",        "ipu1_sel",          cb + 0x3c, 11, 3);
	clks[IMX6QDL_CLK_IPU2_PODF]        = imx_clk_divider("ipu2_podf",        "ipu2_sel",          cb + 0x3c, 16, 3);
	clks[IMX6QDL_CLK_LDB_DI0_DIV_3_5]  = imx_clk_fixed_factor("ldb_di0_div_3_5", "ldb_di0_sel", 2, 7);
	clks[IMX6QDL_CLK_LDB_DI0_PODF]     = imx_clk_divider_np("ldb_di0_podf", "ldb_di0_div_3_5", cb + 0x20, 10, 1);
	clks[IMX6QDL_CLK_LDB_DI1_DIV_3_5]  = imx_clk_fixed_factor("ldb_di1_div_3_5", "ldb_di1_sel", 2, 7);
	clks[IMX6QDL_CLK_LDB_DI1_PODF]     = imx_clk_divider_np("ldb_di1_podf", "ldb_di1_div_3_5", cb + 0x20, 11, 1);
	clks[IMX6QDL_CLK_IPU1_DI0_PRE]     = imx_clk_divider("ipu1_di0_pre",     "ipu1_di0_pre_sel",  cb + 0x34, 3,  3);
	clks[IMX6QDL_CLK_IPU1_DI1_PRE]     = imx_clk_divider("ipu1_di1_pre",     "ipu1_di1_pre_sel",  cb + 0x34, 12, 3);
	clks[IMX6QDL_CLK_IPU2_DI0_PRE]     = imx_clk_divider("ipu2_di0_pre",     "ipu2_di0_pre_sel",  cb + 0x38, 3,  3);
	clks[IMX6QDL_CLK_IPU2_DI1_PRE]     = imx_clk_divider("ipu2_di1_pre",     "ipu2_di1_pre_sel",  cb + 0x38, 12, 3);

	clks[IMX6QDL_CLK_HDMI_IAHB]    = imx_clk_gate2("hdmi_iahb",     "ahb",               cb + 0x70, 0);
	clks[IMX6QDL_CLK_HDMI_ISFR]    = imx_clk_gate2("hdmi_isfr",     "mipi_core_cfg",     cb + 0x70, 4);
	clks[IMX6QDL_CLK_IPU1]         = imx_clk_gate2("ipu1",          "ipu1_podf",         cb + 0x74, 0);
	clks[IMX6QDL_CLK_IPU1_DI0]     = imx_clk_gate2("ipu1_di0",      "ipu1_di0_sel",      cb + 0x74, 2);
	clks[IMX6QDL_CLK_IPU1_DI1]     = imx_clk_gate2("ipu1_di1",      "ipu1_di1_sel",      cb + 0x74, 4);
	clks[IMX6QDL_CLK_IPU2]         = imx_clk_gate2("ipu2",          "ipu2_podf",         cb + 0x74, 6);
	clks[IMX6QDL_CLK_IPU2_DI0]     = imx_clk_gate2("ipu2_di0",      "ipu2_di0_sel",      cb + 0x74, 8);
	clks[IMX6QDL_CLK_LDB_DI0]      = imx_clk_gate2("ldb_di0",       "ldb_di0_podf",      cb + 0x74, 12);
	clks[IMX6QDL_CLK_LDB_DI1]      = imx_clk_gate2("ldb_di1",       "ldb_di1_podf",      cb + 0x74, 14);
	clks[IMX6QDL_CLK_IPU2_DI1]     = imx_clk_gate2("ipu2_di1",      "ipu2_di1_sel",      cb + 0x74, 10);
	clks[IMX6QDL_CLK_MIPI_CORE_CFG] = imx_clk_gate2("mipi_core_cfg", "video_27m", cb + 0x74, 16);
	clks[IMX6QDL_CLK_VIDEO_27M]    = imx_clk_fixed_factor("video_27m", "pll3_pfd1_540m", 1, 20);

	clk_set_parent(clks[IMX6QDL_CLK_IPU1_DI0_SEL], clks[IMX6QDL_CLK_IPU1_DI0_PRE]);
	clk_set_parent(clks[IMX6QDL_CLK_IPU1_DI1_SEL], clks[IMX6QDL_CLK_IPU1_DI1_PRE]);
	clk_set_parent(clks[IMX6QDL_CLK_IPU2_DI0_SEL], clks[IMX6QDL_CLK_IPU2_DI0_PRE]);
	clk_set_parent(clks[IMX6QDL_CLK_IPU2_DI1_SEL], clks[IMX6QDL_CLK_IPU2_DI1_PRE]);

	clk_set_parent(clks[IMX6QDL_CLK_IPU1_DI0_PRE_SEL], clks[IMX6QDL_CLK_PLL5_VIDEO_DIV]);
	clk_set_parent(clks[IMX6QDL_CLK_IPU1_DI1_PRE_SEL], clks[IMX6QDL_CLK_PLL5_VIDEO_DIV]);
	clk_set_parent(clks[IMX6QDL_CLK_IPU2_DI0_PRE_SEL], clks[IMX6QDL_CLK_PLL5_VIDEO_DIV]);
	clk_set_parent(clks[IMX6QDL_CLK_IPU2_DI1_PRE_SEL], clks[IMX6QDL_CLK_PLL5_VIDEO_DIV]);
}

static int imx6_ccm_probe(struct device_d *dev)
{
	struct resource *iores;
	void __iomem *base, *anatop_base, *ccm_base;

	anatop_base = (void *)MX6_ANATOP_BASE_ADDR;
	iores = dev_request_mem_resource(dev, 0);
	if (IS_ERR(iores))
		return PTR_ERR(iores);
	ccm_base = IOMEM(iores->start);

	base = anatop_base;

	/*                   type                               name            parent_name base   div_mask */
	clks[IMX6QDL_CLK_PLL1_SYS]      = imx_clk_pllv3(IMX_PLLV3_SYS,	"pll1_sys",	"osc", base,        0x7f);
	clks[IMX6QDL_CLK_PLL2_BUS]      = imx_clk_pllv3(IMX_PLLV3_GENERIC,	"pll2_bus",	"osc", base + 0x30, 0x1);
	clks[IMX6QDL_CLK_PLL3_USB_OTG]  = imx_clk_pllv3(IMX_PLLV3_USB,	"pll3_usb_otg",	"osc", base + 0x10, 0x3);
	clks[IMX6QDL_CLK_PLL4_AUDIO]    = imx_clk_pllv3(IMX_PLLV3_AV,	"pll4_audio",	"osc", base + 0x70, 0x7f);
	clks[IMX6QDL_CLK_PLL5_VIDEO]    = imx_clk_pllv3(IMX_PLLV3_AV,	"pll5_video",	"osc", base + 0xa0, 0x7f);
	clks[IMX6QDL_CLK_PLL8_MLB]      = imx_clk_pllv3(IMX_PLLV3_MLB,	"pll8_mlb",	"osc", base + 0xd0, 0x0);
	clks[IMX6QDL_CLK_PLL7_USB_HOST] = imx_clk_pllv3(IMX_PLLV3_USB,	"pll7_usb_host","osc", base + 0x20, 0x3);
	clks[IMX6QDL_CLK_PLL6_ENET]     = imx_clk_pllv3(IMX_PLLV3_ENET,	"pll6_enet",	"osc", base + 0xe0, 0x3);

	clks[IMX6QDL_CLK_USBPHY1] = imx_clk_gate("usbphy1", "pll3_usb_otg", base + 0x10, 6);
	clks[IMX6QDL_CLK_USBPHY2] = imx_clk_gate("usbphy2", "pll7_usb_host", base + 0x20, 6);

	clks[IMX6QDL_CLK_SATA_REF] = imx_clk_fixed_factor("sata_ref", "pll6_enet", 1, 5);
	clks[IMX6QDL_CLK_PCIE_REF] = imx_clk_fixed_factor("pcie_ref", "pll6_enet", 1, 4);
	clks[IMX6QDL_CLK_SATA_REF_100M] = imx_clk_gate("sata_ref_100m", "sata_ref", base + 0xe0, 20);
	clks[IMX6QDL_CLK_PCIE_REF_125M] = imx_clk_gate("pcie_ref_125m", "pcie_ref", base + 0xe0, 19);

	clks[IMX6QDL_CLK_ENET_REF] = imx_clk_divider_table("enet_ref", "pll6_enet", base + 0xe0, 0, 2, clk_enet_ref_table);

	clks[IMX6QDL_CLK_LVDS1_SEL] = imx_clk_mux("lvds1_sel", base + 0x160, 0, 5, lvds_sels, ARRAY_SIZE(lvds_sels));
	clks[IMX6QDL_CLK_LVDS2_SEL] = imx_clk_mux("lvds2_sel", base + 0x160, 5, 5, lvds_sels, ARRAY_SIZE(lvds_sels));

	clks[IMX6QDL_CLK_LVDS1_GATE] = imx_clk_gate_exclusive("lvds1_gate", "lvds1_sel", base + 0x160, 10, BIT(12));
	clks[IMX6QDL_CLK_LVDS2_GATE] = imx_clk_gate_exclusive("lvds2_gate", "lvds2_sel", base + 0x160, 11, BIT(13));

	/*                                name               parent_name         reg          idx */
	clks[IMX6QDL_CLK_PLL2_PFD0_352M] = imx_clk_pfd("pll2_pfd0_352m", "pll2_bus",     base + 0x100, 0);
	clks[IMX6QDL_CLK_PLL2_PFD1_594M] = imx_clk_pfd("pll2_pfd1_594m", "pll2_bus",     base + 0x100, 1);
	clks[IMX6QDL_CLK_PLL2_PFD2_396M] = imx_clk_pfd("pll2_pfd2_396m", "pll2_bus",     base + 0x100, 2);
	clks[IMX6QDL_CLK_PLL3_PFD0_720M] = imx_clk_pfd("pll3_pfd0_720m", "pll3_usb_otg", base + 0xf0,  0);
	clks[IMX6QDL_CLK_PLL3_PFD1_540M] = imx_clk_pfd("pll3_pfd1_540m", "pll3_usb_otg", base + 0xf0,  1);
	clks[IMX6QDL_CLK_PLL3_PFD2_508M] = imx_clk_pfd("pll3_pfd2_508m", "pll3_usb_otg", base + 0xf0,  2);
	clks[IMX6QDL_CLK_PLL3_PFD3_454M] = imx_clk_pfd("pll3_pfd3_454m", "pll3_usb_otg", base + 0xf0,  3);

	/*                                    name          parent_name          mult div */
	clks[IMX6QDL_CLK_PLL2_198M] = imx_clk_fixed_factor("pll2_198m", "pll2_pfd2_396m", 1, 2);
	clks[IMX6QDL_CLK_PLL3_120M] = imx_clk_fixed_factor("pll3_120m", "pll3_usb_otg",   1, 4);
	clks[IMX6QDL_CLK_PLL3_80M]  = imx_clk_fixed_factor("pll3_80m",  "pll3_usb_otg",   1, 6);
	clks[IMX6QDL_CLK_PLL3_60M]  = imx_clk_fixed_factor("pll3_60m",  "pll3_usb_otg",   1, 8);
	clks[IMX6QDL_CLK_TWD]       = imx_clk_fixed_factor("twd",       "arm",            1, 2);

	base = ccm_base;

	/*                                  name                 reg       shift width parent_names     num_parents */
	clks[IMX6QDL_CLK_STEP]             = imx_clk_mux("step",	         base + 0xc,  8,  1, step_sels,	        ARRAY_SIZE(step_sels));
	clks[IMX6QDL_CLK_PLL1_SW]          = imx_clk_mux("pll1_sw",	         base + 0xc,  2,  1, pll1_sw_sels,      ARRAY_SIZE(pll1_sw_sels));
	clks[IMX6QDL_CLK_PERIPH_PRE]       = imx_clk_mux("periph_pre",       base + 0x18, 18, 2, periph_pre_sels,   ARRAY_SIZE(periph_pre_sels));
	clks[IMX6QDL_CLK_PERIPH2_PRE]      = imx_clk_mux("periph2_pre",      base + 0x18, 21, 2, periph_pre_sels,   ARRAY_SIZE(periph_pre_sels));
	clks[IMX6QDL_CLK_PERIPH_CLK2_SEL]  = imx_clk_mux("periph_clk2_sel",  base + 0x18, 12, 1, periph_clk2_sels,  ARRAY_SIZE(periph_clk2_sels));
	clks[IMX6QDL_CLK_PERIPH2_CLK2_SEL] = imx_clk_mux("periph2_clk2_sel", base + 0x18, 20, 1, periph2_clk2_sels,  ARRAY_SIZE(periph2_clk2_sels));
	clks[IMX6QDL_CLK_AXI_SEL]          = imx_clk_mux("axi_sel",          base + 0x14, 6,  2, axi_sels,          ARRAY_SIZE(axi_sels));
	clks[IMX6QDL_CLK_USDHC1_SEL]       = imx_clk_mux("usdhc1_sel",       base + 0x1c, 16, 1, usdhc_sels,        ARRAY_SIZE(usdhc_sels));
	clks[IMX6QDL_CLK_USDHC2_SEL]       = imx_clk_mux("usdhc2_sel",       base + 0x1c, 17, 1, usdhc_sels,        ARRAY_SIZE(usdhc_sels));
	clks[IMX6QDL_CLK_USDHC3_SEL]       = imx_clk_mux("usdhc3_sel",       base + 0x1c, 18, 1, usdhc_sels,        ARRAY_SIZE(usdhc_sels));
	clks[IMX6QDL_CLK_USDHC4_SEL]       = imx_clk_mux("usdhc4_sel",       base + 0x1c, 19, 1, usdhc_sels,        ARRAY_SIZE(usdhc_sels));
	if (cpu_mx6_is_plus())
		clks[IMX6QDL_CLK_ENFC_SEL]         = imx_clk_mux("enfc_sel",         base + 0x2c, 15, 3, enfc_sels_plus,    ARRAY_SIZE(enfc_sels_plus));
	else
		clks[IMX6QDL_CLK_ENFC_SEL]         = imx_clk_mux("enfc_sel",         base + 0x2c, 16, 2, enfc_sels,         ARRAY_SIZE(enfc_sels));
	clks[IMX6QDL_CLK_EIM_SEL]          = imx_clk_mux("eim_sel",          base + 0x1c, 27, 2, eim_sels,          ARRAY_SIZE(eim_sels));
	clks[IMX6QDL_CLK_EIM_SLOW_SEL]     = imx_clk_mux("eim_slow_sel",     base + 0x1c, 29, 2, eim_sels,          ARRAY_SIZE(eim_slow_sels));
	clks[IMX6QDL_CLK_VDO_AXI_SEL]      = imx_clk_mux("vdo_axi_sel",      base + 0x18, 11, 1, vdo_axi_sels,      ARRAY_SIZE(vdo_axi_sels));
	clks[IMX6QDL_CLK_CKO1_SEL]         = imx_clk_mux("cko1_sel",         base + 0x60, 0,  4, cko1_sels,         ARRAY_SIZE(cko1_sels));
	clks[IMX6QDL_CLK_CKO2_SEL]         = imx_clk_mux("cko2_sel",         base + 0x60, 16, 5, cko2_sels,         ARRAY_SIZE(cko2_sels));
	clks[IMX6QDL_CLK_CKO]              = imx_clk_mux("cko",              base + 0x60, 8,  1, cko_sels,          ARRAY_SIZE(cko_sels));
	clks[IMX6QDL_CLK_PCIE_AXI_SEL]     = imx_clk_mux("pcie_axi_sel",     base + 0x18, 10, 1, pcie_axi_sels,     ARRAY_SIZE(pcie_axi_sels));

	/*                              name         reg       shift width busy: reg, shift parent_names  num_parents */
	clks[IMX6QDL_CLK_PERIPH]  = imx_clk_busy_mux("periph",  base + 0x14, 25,  1,   base + 0x48, 5,  periph_sels,  ARRAY_SIZE(periph_sels));
	clks[IMX6QDL_CLK_PERIPH2] = imx_clk_busy_mux("periph2", base + 0x14, 26,  1,   base + 0x48, 3,  periph2_sels, ARRAY_SIZE(periph2_sels));

	/*                                      name                 parent_name               reg       shift width */
	clks[IMX6QDL_CLK_PERIPH_CLK2]      = imx_clk_divider("periph_clk2",      "periph_clk2_sel",   base + 0x14, 27, 3);
	clks[IMX6QDL_CLK_PERIPH2_CLK2]     = imx_clk_divider("periph2_clk2",     "periph2_clk2_sel",  base + 0x14, 0,  3);
	clks[IMX6QDL_CLK_IPG]              = imx_clk_divider("ipg",              "ahb",               base + 0x14, 8,  2);
	clks[IMX6QDL_CLK_IPG_PER]          = imx_clk_divider("ipg_per",          "ipg",               base + 0x1c, 0,  6);
	clks[IMX6QDL_CLK_CAN_ROOT]         = imx_clk_divider("can_root",         "pll3_usb_otg",      base + 0x20, 2,  6);
	clks[IMX6QDL_CLK_ECSPI_ROOT]       = imx_clk_divider("ecspi_root",       "pll3_60m",          base + 0x38, 19, 6);
	clks[IMX6QDL_CLK_UART_SERIAL_PODF] = imx_clk_divider("uart_serial_podf", "pll3_80m",          base + 0x24, 0,  6);
	clks[IMX6QDL_CLK_USDHC1_PODF]      = imx_clk_divider("usdhc1_podf",      "usdhc1_sel",        base + 0x24, 11, 3);
	clks[IMX6QDL_CLK_USDHC2_PODF]      = imx_clk_divider("usdhc2_podf",      "usdhc2_sel",        base + 0x24, 16, 3);
	clks[IMX6QDL_CLK_USDHC3_PODF]      = imx_clk_divider("usdhc3_podf",      "usdhc3_sel",        base + 0x24, 19, 3);
	clks[IMX6QDL_CLK_USDHC4_PODF]      = imx_clk_divider("usdhc4_podf",      "usdhc4_sel",        base + 0x24, 22, 3);
	clks[IMX6QDL_CLK_ENFC_PRED]        = imx_clk_divider("enfc_pred",        "enfc_sel",          base + 0x2c, 18, 3);
	clks[IMX6QDL_CLK_ENFC_PODF]        = imx_clk_divider("enfc_podf",        "enfc_pred",         base + 0x2c, 21, 6);
	clks[IMX6QDL_CLK_EIM_PODF]         = imx_clk_divider("eim_podf",         "eim_sel",           base + 0x1c, 20, 3);
	clks[IMX6QDL_CLK_EIM_SLOW_PODF]    = imx_clk_divider("eim_slow_podf",    "eim_slow_sel",      base + 0x1c, 23, 3);
	clks[IMX6QDL_CLK_CKO1_PODF]        = imx_clk_divider("cko1_podf",        "cko1_sel",          base + 0x60, 4,  3);
	clks[IMX6QDL_CLK_CKO2_PODF]        = imx_clk_divider("cko2_podf",        "cko2_sel",          base + 0x60, 21, 3);

	/*                                            name                  parent_name         reg        shift width busy: reg, shift */
	clks[IMX6QDL_CLK_AXI]               = imx_clk_busy_divider("axi",               "axi_sel",     base + 0x14, 16,  3,   base + 0x48, 0);
	clks[IMX6QDL_CLK_MMDC_CH0_AXI_PODF] = imx_clk_busy_divider("mmdc_ch0_axi_podf", "periph",      base + 0x14, 19,  3,   base + 0x48, 4);
	clks[IMX6QDL_CLK_MMDC_CH1_AXI_PODF] = imx_clk_busy_divider("mmdc_ch1_axi_podf", "periph2",     base + 0x14, 3,   3,   base + 0x48, 2);
	clks[IMX6QDL_CLK_ARM]               = imx_clk_busy_divider("arm",               "pll1_sw",     base + 0x10, 0,   3,   base + 0x48, 16);
	clks[IMX6QDL_CLK_AHB]               = imx_clk_busy_divider("ahb",               "periph",      base + 0x14, 10,  3,   base + 0x48, 1);

	/*                                            name             parent_name          reg         shift */
	clks[IMX6QDL_CLK_APBH_DMA]     = imx_clk_gate2("apbh_dma",      "usdhc3",            base + 0x68, 4);
	clks[IMX6QDL_CLK_CAAM_MEM]     = imx_clk_gate2("caam_mem",      "ahb",               base + 0x68, 8);
	clks[IMX6QDL_CLK_CAAM_ACLK]    = imx_clk_gate2("caam_aclk",     "ahb",               base + 0x68, 10);
	clks[IMX6QDL_CLK_CAAM_IPG]     = imx_clk_gate2("caam_ipg",      "ipg",               base + 0x68, 12);
	clks[IMX6QDL_CLK_ECSPI1]       = imx_clk_gate2("ecspi1",        "ecspi_root",        base + 0x6c, 0);
	clks[IMX6QDL_CLK_ECSPI2]       = imx_clk_gate2("ecspi2",        "ecspi_root",        base + 0x6c, 2);
	clks[IMX6QDL_CLK_ECSPI3]       = imx_clk_gate2("ecspi3",        "ecspi_root",        base + 0x6c, 4);
	clks[IMX6QDL_CLK_ECSPI4]       = imx_clk_gate2("ecspi4",        "ecspi_root",        base + 0x6c, 6);
	if (cpu_mx6_is_mx6dl())
		clks[IMX6DL_CLK_I2C4]  = imx_clk_gate2("i2c4",          "ipg_per",           base + 0x6c, 8);
	else
		clks[IMX6Q_CLK_ECSPI5] = imx_clk_gate2("ecspi5",        "ecspi_root",        base + 0x6c, 8);
	clks[IMX6QDL_CLK_ENET]         = imx_clk_gate2("enet",          "ipg",               base + 0x6c, 10);
	clks[IMX6QDL_CLK_GPT_IPG]      = imx_clk_gate2("gpt_ipg",       "ipg",               base + 0x6c, 20);
	clks[IMX6QDL_CLK_GPT_IPG_PER]  = imx_clk_gate2("gpt_ipg_per",   "ipg_per",           base + 0x6c, 22);
	clks[IMX6QDL_CLK_I2C1]         = imx_clk_gate2("i2c1",          "ipg_per",           base + 0x70, 6);
	clks[IMX6QDL_CLK_I2C2]         = imx_clk_gate2("i2c2",          "ipg_per",           base + 0x70, 8);
	clks[IMX6QDL_CLK_I2C3]         = imx_clk_gate2("i2c3",          "ipg_per",           base + 0x70, 10);
	clks[IMX6QDL_CLK_IIM]          = imx_clk_gate2("iim",           "ipg",               base + 0x70, 12);
	clks[IMX6QDL_CLK_ENFC]         = imx_clk_gate2("enfc",          "enfc_podf",         base + 0x70, 14);
	clks[IMX6QDL_CLK_PCIE_AXI]     = imx_clk_gate2("pcie_axi",      "pcie_axi_sel",      base + 0x78, 0);
	clks[IMX6QDL_CLK_PER1_BCH]     = imx_clk_gate2("per1_bch",      "usdhc3",            base + 0x78, 12);
	clks[IMX6QDL_CLK_PWM1]         = imx_clk_gate2("pwm1",          "ipg_per",           base + 0x78, 16);
	clks[IMX6QDL_CLK_PWM2]         = imx_clk_gate2("pwm2",          "ipg_per",           base + 0x78, 18);
	clks[IMX6QDL_CLK_PWM3]         = imx_clk_gate2("pwm3",          "ipg_per",           base + 0x78, 20);
	clks[IMX6QDL_CLK_PWM4]         = imx_clk_gate2("pwm4",          "ipg_per",           base + 0x78, 22);
	clks[IMX6QDL_CLK_GPMI_BCH_APB] = imx_clk_gate2("gpmi_bch_apb",  "usdhc3",            base + 0x78, 24);
	clks[IMX6QDL_CLK_GPMI_BCH]     = imx_clk_gate2("gpmi_bch",      "usdhc4",            base + 0x78, 26);
	clks[IMX6QDL_CLK_GPMI_IO]      = imx_clk_gate2("gpmi_io",       "enfc",              base + 0x78, 28);
	clks[IMX6QDL_CLK_GPMI_APB]     = imx_clk_gate2("gpmi_apb",      "usdhc3",            base + 0x78, 30);
	clks[IMX6QDL_CLK_SATA]         = imx_clk_gate2("sata",          "ipg",               base + 0x7c, 4);
	clks[IMX6QDL_CLK_UART_IPG]     = imx_clk_gate2("uart_ipg",      "ipg",               base + 0x7c, 24);
	clks[IMX6QDL_CLK_UART_SERIAL]  = imx_clk_gate2("uart_serial",   "uart_serial_podf",  base + 0x7c, 26);
	clks[IMX6QDL_CLK_USBOH3]       = imx_clk_gate2("usboh3",        "ipg",               base + 0x80, 0);
	clks[IMX6QDL_CLK_USDHC1]       = imx_clk_gate2("usdhc1",        "usdhc1_podf",       base + 0x80, 2);
	clks[IMX6QDL_CLK_USDHC2]       = imx_clk_gate2("usdhc2",        "usdhc2_podf",       base + 0x80, 4);
	clks[IMX6QDL_CLK_USDHC3]       = imx_clk_gate2("usdhc3",        "usdhc3_podf",       base + 0x80, 6);
	clks[IMX6QDL_CLK_USDHC4]       = imx_clk_gate2("usdhc4",        "usdhc4_podf",       base + 0x80, 8);
	clks[IMX6QDL_CLK_EIM_SLOW]     = imx_clk_gate2("eim_slow",      "eim_slow_podf",     base + 0x80, 10);
	clks[IMX6QDL_CLK_CKO1]         = imx_clk_gate("cko1",           "cko1_podf",         base + 0x60, 7);
	clks[IMX6QDL_CLK_CKO2]         = imx_clk_gate("cko2",           "cko2_podf",         base + 0x60, 24);

	clkdev_add_physbase(clks[IMX6QDL_CLK_IPG], MX6_OCOTP_BASE_ADDR, NULL);

	disable_anatop_clocks(anatop_base);

	imx6q_mmdc_ch1_mask_handshake(ccm_base);

	if (IS_ENABLED(CONFIG_DRIVER_VIDEO_IMX_IPUV3))
		imx6_add_video_clks(anatop_base, ccm_base, dev->device_node);

	writel(0xffffffff, ccm_base + CCGR0);
	writel(0xf0ffffff, ccm_base + CCGR1); /* gate GPU3D, GPU2D */
	writel(0xffffffff, ccm_base + CCGR2);
	writel(0x3fffffff, ccm_base + CCGR3); /* gate OpenVG */
	if (IS_ENABLED(CONFIG_PCI_IMX6))
		writel(0xffffffff, ccm_base + CCGR4);
	else
		writel(0xfffffffc, ccm_base + CCGR4); /* gate PCIe */
	writel(0xffffffff, ccm_base + CCGR5);
	writel(0xffff3fff, ccm_base + CCGR6); /* gate VPU */
	writel(0xffffffff, ccm_base + CCGR7);

	clk_data.clks = clks;
	clk_data.clk_num = IMX6QDL_CLK_END;
	of_clk_add_provider(dev->device_node, of_clk_src_onecell_get, &clk_data);

	clk_enable(clks[IMX6QDL_CLK_MMDC_CH0_AXI_PODF]);
	clk_enable(clks[IMX6QDL_CLK_PLL6_ENET]);
	clk_enable(clks[IMX6QDL_CLK_SATA_REF_100M]);

	clk_set_parent(clks[IMX6QDL_CLK_LVDS1_SEL], clks[IMX6QDL_CLK_SATA_REF_100M]);

	/*
	 * The gpmi needs 100MHz frequency in the EDO/Sync mode,
	 * We can not get the 100MHz from the pll2_pfd0_352m.
	 * So choose pll2_pfd2_396m as enfc_sel's parent.
	 */
	clk_set_parent(clks[IMX6QDL_CLK_ENFC_SEL], clks[IMX6QDL_CLK_PLL2_PFD2_396M]);

	return 0;
}

static __maybe_unused struct of_device_id imx6_ccm_dt_ids[] = {
	{
		.compatible = "fsl,imx6q-ccm",
	}, {
		/* sentinel */
	}
};

static struct driver_d imx6_ccm_driver = {
	.probe	= imx6_ccm_probe,
	.name	= "imx6-ccm",
	.of_compatible = DRV_OF_COMPAT(imx6_ccm_dt_ids),
};

core_platform_driver(imx6_ccm_driver);

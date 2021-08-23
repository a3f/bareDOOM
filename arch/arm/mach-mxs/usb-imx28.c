// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 2011 Sascha Hauer <s.hauer@pengutronix.de>, Pengutronix

/* i.MX28 USBPHY setup */

#include <common.h>
#include <io.h>
#include <errno.h>
#include <mach/imx28-regs.h>
#include <mach/usb.h>

#define POWER_CTRL			(IMX_POWER_BASE + 0x0)
#define POWER_CTRL_CLKGATE		0x40000000

#define POWER_STS			(IMX_POWER_BASE + 0xc0)
#define POWER_STS_VBUSVALID		0x00000002
#define POWER_STS_BVALID		0x00000004
#define POWER_STS_AVALID		0x00000008

#define POWER_DEBUG			(IMX_POWER_BASE + 0x110)
#define POWER_DEBUG_BVALIDPIOLOCK	0x00000002
#define POWER_DEBUG_AVALIDPIOLOCK	0x00000004
#define POWER_DEBUG_VBUSVALIDPIOLOCK	0x00000008

#define USBPHY_PWD			0x0

#define USBPHY_CTRL			0x30
#define USBPHY_CTRL_SFTRST		(1 << 31)
#define USBPHY_CTRL_CLKGATE		(1 << 30)
#define USBPHY_CTRL_ENUTMILEVEL3	(1 << 15)
#define USBPHY_CTRL_ENUTMILEVEL2	(1 << 14)

#define CLK_PLL0CTRL0			(IMX_CCM_BASE + 0x0)
#define CLK_PLL1CTRL0			(IMX_CCM_BASE + 0x20)
#define PLLCTRL0_EN_USB_CLKS		(1 << 18)
#define PLLCTRL0_POWER		(1 << 17)

#define DIGCTRL_CTRL			(IMX_DIGCTL_BASE + 0x0)
#define DIGCTL_CTRL_USB0_CLKGATE	(1 << 2)
#define DIGCTL_CTRL_USB1_CLKGATE	(1 << 16)

#define SET	0x4
#define CLR	0x8

static void imx28_usb_phy_reset(void __iomem *phybase)
{
	/* Reset USBPHY module */
	writel(USBPHY_CTRL_SFTRST, phybase + USBPHY_CTRL + SET);
	udelay(10);
	writel(USBPHY_CTRL_CLKGATE | USBPHY_CTRL_SFTRST,
			phybase + USBPHY_CTRL + CLR);
}

static void imx28_usb_phy_enable(void __iomem *phybase)
{
	/* Power up the PHY */
	writel(0, phybase + USBPHY_PWD);

	writel(USBPHY_CTRL_ENUTMILEVEL3 | USBPHY_CTRL_ENUTMILEVEL2 | 1,
			phybase + USBPHY_CTRL + SET);
}

int imx28_usb_phy0_enable(void)
{
	imx28_usb_phy_reset((void *)IMX_USBPHY0_BASE);

	/* Turn on the USB clocks */
	writel(PLLCTRL0_EN_USB_CLKS | PLLCTRL0_POWER, CLK_PLL0CTRL0 + SET);

	writel(DIGCTL_CTRL_USB0_CLKGATE, DIGCTRL_CTRL + CLR);

	imx28_usb_phy_enable((void *)IMX_USBPHY0_BASE);

	return 0;
}

int imx28_usb_phy1_enable(void)
{
	imx28_usb_phy_reset((void *)IMX_USBPHY1_BASE);

	/* Turn on the USB clocks */
	writel(PLLCTRL0_EN_USB_CLKS | PLLCTRL0_POWER, CLK_PLL1CTRL0 + SET);

	writel(DIGCTL_CTRL_USB1_CLKGATE, DIGCTRL_CTRL + CLR);

	imx28_usb_phy_enable((void *)IMX_USBPHY1_BASE);

	return 0;
}

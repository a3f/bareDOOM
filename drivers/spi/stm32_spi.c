// SPDX-License-Identifier: GPL-2.0+ OR BSD-3-Clause
/*
 * Copyright (C) 2019, STMicroelectronics - All Rights Reserved
 *
 * Driver for STMicroelectronics Serial peripheral interface (SPI)
 */

#include <common.h>
#include <linux/clk.h>
#include <driver.h>
#include <init.h>
#include <errno.h>
#include <linux/reset.h>
#include <linux/spi/spi-mem.h>
#include <spi/spi.h>
#include <linux/bitops.h>
#include <clock.h>
#include <gpio.h>
#include <of_gpio.h>
#include <linux/bitfield.h>
#include <linux/iopoll.h>

/* STM32F4 SPI registers */
#define STM32F4_SPI_CR1			0x00
#define STM32F4_SPI_CR2			0x04
#define STM32F4_SPI_SR			0x08
#define STM32F4_SPI_DR			0x0C
#define STM32F4_SPI_I2SCFGR		0x1C

/* STM32F4_SPI_CR1 bit fields */
#define STM32F4_SPI_CR1_CPHA		BIT(0)
#define STM32F4_SPI_CR1_CPOL		BIT(1)
#define STM32F4_SPI_CR1_MSTR		BIT(2)
#define STM32F4_SPI_CR1_BR_SHIFT	3
#define STM32F4_SPI_CR1_BR		GENMASK(5, 3)
#define STM32F4_SPI_CR1_SPE		BIT(6)
#define STM32F4_SPI_CR1_LSBFRST		BIT(7)
#define STM32F4_SPI_CR1_SSI		BIT(8)
#define STM32F4_SPI_CR1_SSM		BIT(9)
#define STM32F4_SPI_CR1_RXONLY		BIT(10)
#define STM32F4_SPI_CR1_DFF		BIT(11)
#define STM32F4_SPI_CR1_CRCNEXT		BIT(12)
#define STM32F4_SPI_CR1_CRCEN		BIT(13)
#define STM32F4_SPI_CR1_BIDIOE		BIT(14)
#define STM32F4_SPI_CR1_BIDIMODE	BIT(15)
#define STM32F4_SPI_CR1_BR_MIN		0
#define STM32F4_SPI_CR1_BR_MAX		(GENMASK(5, 3) >> 3)

/* STM32F4_SPI_CR2 bit fields */
#define STM32F4_SPI_CR2_RXDMAEN		BIT(0)
#define STM32F4_SPI_CR2_TXDMAEN		BIT(1)
#define STM32F4_SPI_CR2_SSOE		BIT(2)
#define STM32F4_SPI_CR2_FRF		BIT(4)
#define STM32F4_SPI_CR2_ERRIE		BIT(5)
#define STM32F4_SPI_CR2_RXNEIE		BIT(6)
#define STM32F4_SPI_CR2_TXEIE		BIT(7)

/* STM32F4_SPI_SR bit fields */
#define STM32F4_SPI_SR_RXNE		BIT(0)
#define STM32F4_SPI_SR_TXE		BIT(1)
#define STM32F4_SPI_SR_CHSIDE		BIT(2)
#define STM32F4_SPI_SR_UDR		BIT(3)
#define STM32F4_SPI_SR_CRCERR		BIT(4)
#define STM32F4_SPI_SR_MODF		BIT(5)
#define STM32F4_SPI_SR_OVR		BIT(6)
#define STM32F4_SPI_SR_BSY		BIT(7)
#define STM32F4_SPI_SR_FRE		BIT(8)

/* STM32F4_SPI_I2SCFGR bit fields */
#define STM32F4_SPI_I2SCFGR_I2SMOD	BIT(11)

/* STM32F4 SPI Baud Rate min/max divisor */
#define STM32F4_SPI_BR_DIV_MIN		(2 << STM32F4_SPI_CR1_BR_MIN)
#define STM32F4_SPI_BR_DIV_MAX		(2 << STM32F4_SPI_CR1_BR_MAX)

/* STM32H7 SPI registers */
#define STM32H7_SPI_CR1		0x00
#define STM32H7_SPI_CR2		0x04
#define STM32H7_SPI_CFG1	0x08
#define STM32H7_SPI_CFG2	0x0C
#define STM32H7_SPI_SR		0x14
#define STM32H7_SPI_IFCR	0x18
#define STM32H7_SPI_TXDR	0x20
#define STM32H7_SPI_RXDR	0x30
#define STM32H7_SPI_I2SCFGR	0x50

/* STM32H7_SPI_CR1 bit fields */
#define STM32H7_SPI_CR1_SPE		BIT(0)
#define STM32H7_SPI_CR1_MASRX		BIT(8)
#define STM32H7_SPI_CR1_CSTART		BIT(9)
#define STM32H7_SPI_CR1_CSUSP		BIT(10)
#define STM32H7_SPI_CR1_HDDIR		BIT(11)
#define STM32H7_SPI_CR1_SSI		BIT(12)

/* STM32H7_SPI_CR2 bit fields */
#define STM32H7_SPI_CR2_TSIZE		GENMASK(15, 0)
#define STM32H7_SPI_TSIZE_MAX		FIELD_GET(STM32H7_SPI_CR2_TSIZE, STM32H7_SPI_CR2_TSIZE)

/* STM32H7_SPI_CFG1 bit fields */
#define STM32H7_SPI_CFG1_DSIZE		GENMASK(4, 0)
#define STM32H7_SPI_CFG1_DSIZE_MIN	3
#define STM32H7_SPI_CFG1_FTHLV_SHIFT	5
#define STM32H7_SPI_CFG1_FTHLV		GENMASK(8, 5)
#define STM32H7_SPI_CFG1_MBR_SHIFT	28
#define STM32H7_SPI_CFG1_MBR		GENMASK(30, 28)
#define STM32H7_SPI_CFG1_MBR_MIN	0
#define STM32H7_SPI_CFG1_MBR_MAX	(STM32H7_SPI_CFG1_MBR >> STM32H7_SPI_CFG1_MBR_SHIFT)

/* STM32H7_SPI_CFG2 bit fields */
#define STM32H7_SPI_CFG2_COMM_SHIFT	17
#define STM32H7_SPI_CFG2_COMM		GENMASK(18, 17)
#define STM32H7_SPI_CFG2_MASTER		BIT(22)
#define STM32H7_SPI_CFG2_LSBFRST	BIT(23)
#define STM32H7_SPI_CFG2_CPHA		BIT(24)
#define STM32H7_SPI_CFG2_CPOL		BIT(25)
#define STM32H7_SPI_CFG2_SSM		BIT(26)
#define STM32H7_SPI_CFG2_AFCNTR		BIT(31)

/* STM32H7_SPI_SR bit fields */
#define STM32H7_SPI_SR_RXP		BIT(0)
#define STM32H7_SPI_SR_TXP		BIT(1)
#define STM32H7_SPI_SR_EOT		BIT(3)
#define STM32H7_SPI_SR_TXTF		BIT(4)
#define STM32H7_SPI_SR_OVR		BIT(6)
#define STM32H7_SPI_SR_SUSP		BIT(11)
#define STM32H7_SPI_SR_RXPLVL_SHIFT	13
#define STM32H7_SPI_SR_RXPLVL		GENMASK(14, 13)
#define STM32H7_SPI_SR_RXWNE		BIT(15)

/* STM32H7_SPI_IFCR bit fields */
#define STM32H7_SPI_IFCR_ALL		GENMASK(11, 3)

/* STM32H7_SPI_I2SCFGR bit fields */
#define STM32H7_SPI_I2SCFGR_I2SMOD	BIT(0)

/* SPI Master Baud Rate min/max divisor */
#define STM32H7_MBR_DIV_MIN		(2 << STM32H7_SPI_CFG1_MBR_MIN)
#define STM32H7_MBR_DIV_MAX		(2 << STM32H7_SPI_CFG1_MBR_MAX)

/* SPI Communication mode */
#define STM32H7_SPI_FULL_DUPLEX		0
#define STM32H7_SPI_SIMPLEX_TX		1
#define STM32H7_SPI_SIMPLEX_RX		2
#define STM32H7_SPI_HALF_DUPLEX		3

/* SPI Communication type */
enum spi_comm_type {
	SPI_FULL_DUPLEX,
	SPI_SIMPLEX_TX,
	SPI_SIMPLEX_RX,
	SPI_3WIRE_TX,
	SPI_3WIRE_RX,
};

struct stm32_spi_cfg;

struct stm32_spi_priv {
	struct spi_master	master;
	int			*cs_gpios;
	void __iomem		*base;
	struct clk		*clk;
	ulong			bus_clk_rate;
	unsigned int		fifo_size;
	unsigned int		cur_bpw;
	unsigned int		cur_hz;
	unsigned int		cur_xferlen;	/* current transfer length in bytes */
	unsigned int		tx_len;		/* number of data to be written in bytes */
	unsigned int		rx_len;		/* number of data to be read in bytes */
	const void		*tx_buf;	/* data to be written, or NULL */
	void			*rx_buf;	/* data to be read, or NULL */
	enum spi_comm_type	cur_mode;
	const struct stm32_spi_cfg *cfg;
};

/**
 * struct stm32_spi_reg - stm32 SPI register & bitfield desc
 * @reg:		register offset
 * @mask:		bitfield mask
 * @shift:		left shift
 */
struct stm32_spi_reg {
	int reg;
	int mask;
	int shift;
};

/**
 * struct stm32_spi_regspec - stm32 registers definition, compatible dependent data
 * @en: enable register and SPI enable bit
 * @cpol: clock polarity register and polarity bit
 * @cpha: clock phase register and phase bit
 * @lsb_first: LSB transmitted first register and bit
 * @br: baud rate register and bitfields
 */
struct stm32_spi_regspec {
	const struct stm32_spi_reg en;
	const struct stm32_spi_reg cpol;
	const struct stm32_spi_reg cpha;
	const struct stm32_spi_reg lsb_first;
	const struct stm32_spi_reg br;
};

/**
 * struct stm32_spi_cfg - stm32 compatible configuration data
 * @regs: registers descriptions
 * @get_fifo_size: routine to get fifo size
 * @get_bpw_mask: routine to get bits per word mask
 * @config: routine to configure controller as SPI Master
 * @set_bpw: routine to configure registers to for bits per word
 * @set_mode: routine to configure registers to desired mode
 * @set_number_of_data: optional routine to configure registers to desired
 * number of data (if driver has this functionality)
 * @transfer_one: routine to configure interrupts for driver
 * @stop_transfer: stop SPI transfer
 * @baud_rate_div_min: minimum baud rate divisor
 * @baud_rate_div_max: maximum baud rate divisor
 * @has_fifo: boolean to know if fifo is used for driver
 * @has_startbit: boolean to know if start bit is used to start transfer
 */
struct stm32_spi_cfg {
	const struct stm32_spi_regspec *regs;
	void (*stop_transfer)(struct stm32_spi_priv *priv);
	int (*get_fifo_size)(struct stm32_spi_priv *priv);
	int (*get_bpw_mask)(struct stm32_spi_priv *priv);
	void (*config)(struct stm32_spi_priv *priv);
	void (*set_bpw)(struct stm32_spi_priv *priv);
	int (*set_mode)(struct stm32_spi_priv *priv, enum spi_comm_type comm_type);
	int (*set_number_of_data)(struct stm32_spi_priv *priv, u32 length);
	const struct spi_controller_mem_ops *mem_ops;
	int (*transfer_one)(struct stm32_spi_priv *priv,
			    struct spi_transfer *t);
	unsigned int baud_rate_div_min;
	unsigned int baud_rate_div_max;
	bool has_fifo;
};

static const struct stm32_spi_regspec stm32f4_spi_regspec = {
	.en = { STM32F4_SPI_CR1, STM32F4_SPI_CR1_SPE },

	.cpol = { STM32F4_SPI_CR1, STM32F4_SPI_CR1_CPOL },
	.cpha = { STM32F4_SPI_CR1, STM32F4_SPI_CR1_CPHA },
	.lsb_first = { STM32F4_SPI_CR1, STM32F4_SPI_CR1_LSBFRST },
	.br = { STM32F4_SPI_CR1, STM32F4_SPI_CR1_BR, STM32F4_SPI_CR1_BR_SHIFT },
};

static const struct stm32_spi_regspec stm32h7_spi_regspec = {
	/* SPI data transfer is enabled but spi_ker_ck is idle.
	 * CFG1 and CFG2 registers are write protected when SPE is enabled.
	 */
	.en = { STM32H7_SPI_CR1, STM32H7_SPI_CR1_SPE },

	.cpol = { STM32H7_SPI_CFG2, STM32H7_SPI_CFG2_CPOL },
	.cpha = { STM32H7_SPI_CFG2, STM32H7_SPI_CFG2_CPHA },
	.lsb_first = { STM32H7_SPI_CFG2, STM32H7_SPI_CFG2_LSBFRST },
	.br = { STM32H7_SPI_CFG1, STM32H7_SPI_CFG1_MBR,
		STM32H7_SPI_CFG1_MBR_SHIFT },
};

static inline struct stm32_spi_priv *to_stm32_spi_priv(struct spi_master *master)
{
	return container_of(master, struct stm32_spi_priv, master);
}

static void stm32f4_spi_write_tx(struct stm32_spi_priv *priv)
{
	if ((priv->tx_len > 0) && (readl(priv->base + STM32F4_SPI_SR) &
				  STM32F4_SPI_SR_TXE)) {
		u32 offs = priv->cur_xferlen - priv->tx_len;

		if (!priv->tx_buf) {
			writeb(0x00, priv->base + STM32F4_SPI_DR);
			priv->tx_len -= sizeof(u8);
		} else if (priv->cur_bpw == 16 &&
		    IS_ALIGNED((uintptr_t)(priv->tx_buf + offs), sizeof(u16))) {
			const u16 *tx_buf16 = (const u16 *)(priv->tx_buf + offs);

			writew(*tx_buf16, priv->base + STM32F4_SPI_DR);
			priv->tx_len -= sizeof(u16);
		} else {
			const u8 *tx_buf8 = (const u8 *)(priv->tx_buf + offs);

			writeb(*tx_buf8, priv->base + STM32F4_SPI_DR);
			priv->tx_len -= sizeof(u8);
		}
	}
}

static void stm32f4_spi_read_rx(struct stm32_spi_priv *priv)
{
	if ((priv->rx_len > 0) && (readl(priv->base + STM32F4_SPI_SR) &
				  STM32F4_SPI_SR_RXNE)) {
		u32 offs = priv->cur_xferlen - priv->rx_len;

		if (!priv->rx_buf) {
			readb(priv->base + STM32F4_SPI_DR);
			priv->rx_len -= sizeof(u8);
		} else if (priv->cur_bpw == 16 &&
		    IS_ALIGNED((uintptr_t)(priv->rx_buf + offs), sizeof(u16))) {
			u16 *rx_buf16 = (u16 *)(priv->rx_buf + offs);

			*rx_buf16 = readw(priv->base + STM32F4_SPI_DR);
			priv->rx_len -= sizeof(u16);
		} else {
			u8 *rx_buf8 = (u8 *)(priv->rx_buf + offs);

			*rx_buf8 = readb(priv->base + STM32F4_SPI_DR);
			priv->rx_len -= sizeof(u8);
		}
	}
}

static void stm32h7_spi_write_txfifo(struct stm32_spi_priv *priv)
{
	while ((priv->tx_len > 0) &&
	       (readl(priv->base + STM32H7_SPI_SR) & STM32H7_SPI_SR_TXP)) {
		u32 offs = priv->cur_xferlen - priv->tx_len;

		if (priv->tx_len >= sizeof(u32) &&
		    IS_ALIGNED((uintptr_t)(priv->tx_buf + offs), sizeof(u32))) {
			const u32 *tx_buf32 = (const u32 *)(priv->tx_buf + offs);

			writel(*tx_buf32, priv->base + STM32H7_SPI_TXDR);
			priv->tx_len -= sizeof(u32);
		} else if (priv->tx_len >= sizeof(u16) &&
			   IS_ALIGNED((uintptr_t)(priv->tx_buf + offs), sizeof(u16))) {
			const u16 *tx_buf16 = (const u16 *)(priv->tx_buf + offs);

			writew(*tx_buf16, priv->base + STM32H7_SPI_TXDR);
			priv->tx_len -= sizeof(u16);
		} else {
			const u8 *tx_buf8 = (const u8 *)(priv->tx_buf + offs);

			writeb(*tx_buf8, priv->base + STM32H7_SPI_TXDR);
			priv->tx_len -= sizeof(u8);
		}
	}

	dev_dbg(priv->master.dev, "%d bytes left\n", priv->tx_len);
}

static void stm32h7_spi_read_rxfifo(struct stm32_spi_priv *priv)
{
	u32 sr = readl(priv->base + STM32H7_SPI_SR);
	u32 rxplvl = (sr & STM32H7_SPI_SR_RXPLVL) >> STM32H7_SPI_SR_RXPLVL_SHIFT;

	while ((priv->rx_len > 0) &&
	       ((sr & STM32H7_SPI_SR_RXP) ||
	       ((sr & STM32H7_SPI_SR_EOT) && ((sr & STM32H7_SPI_SR_RXWNE) || (rxplvl > 0))))) {
		u32 offs = priv->cur_xferlen - priv->rx_len;

		if (IS_ALIGNED((uintptr_t)(priv->rx_buf + offs), sizeof(u32)) &&
		    (priv->rx_len >= sizeof(u32) || (sr & STM32H7_SPI_SR_RXWNE))) {
			u32 *rx_buf32 = (u32 *)(priv->rx_buf + offs);

			*rx_buf32 = readl(priv->base + STM32H7_SPI_RXDR);
			priv->rx_len -= sizeof(u32);
		} else if (IS_ALIGNED((uintptr_t)(priv->rx_buf + offs), sizeof(u16)) &&
			   (priv->rx_len >= sizeof(u16) ||
			    (!(sr & STM32H7_SPI_SR_RXWNE) &&
			    (rxplvl >= 2 || priv->cur_bpw > 8)))) {
			u16 *rx_buf16 = (u16 *)(priv->rx_buf + offs);

			*rx_buf16 = readw(priv->base + STM32H7_SPI_RXDR);
			priv->rx_len -= sizeof(u16);
		} else {
			u8 *rx_buf8 = (u8 *)(priv->rx_buf + offs);

			*rx_buf8 = readb(priv->base + STM32H7_SPI_RXDR);
			priv->rx_len -= sizeof(u8);
		}

		sr = readl(priv->base + STM32H7_SPI_SR);
		rxplvl = (sr & STM32H7_SPI_SR_RXPLVL) >> STM32H7_SPI_SR_RXPLVL_SHIFT;
	}

	dev_dbg(priv->master.dev, "%d bytes left\n", priv->rx_len);
}

static void stm32_spi_enable(struct stm32_spi_priv *priv)
{
	setbits_le32(priv->base + priv->cfg->regs->en.reg, priv->cfg->regs->en.mask);
}

static void stm32_spi_disable(struct stm32_spi_priv *priv)
{
	clrbits_le32(priv->base + priv->cfg->regs->en.reg, priv->cfg->regs->en.mask);
}

static void stm32f4_spi_stop_transfer(struct stm32_spi_priv *priv)
{
	struct device_d *dev = priv->master.dev;
	u32 cr1, sr;
	int ret;

	dev_dbg(dev, "%s\n", __func__);

	cr1 = readl(priv->base + STM32F4_SPI_CR1);

	if (!(cr1 & STM32F4_SPI_CR1_SPE))
		return;

	/* Wait on !busy or suspend the flow */
	ret = readl_poll_timeout(priv->base + STM32F4_SPI_SR, sr,
				 !(sr & STM32F4_SPI_SR_BSY), USEC_PER_SEC);
	if (ret < 0)
		dev_warn(dev, "disabling condition timeout\n");

	clrbits_le32(priv->base + STM32F4_SPI_CR1, STM32F4_SPI_CR1_SPE);

	/* Sequence to clear OVR flag */
	readl(priv->base + STM32F4_SPI_DR);
	readl(priv->base + STM32F4_SPI_SR);
}

static void stm32h7_spi_stop_transfer(struct stm32_spi_priv *priv)
{
	struct device_d *dev = priv->master.dev;
	u32 cr1, sr;
	int ret;

	dev_dbg(dev, "%s\n", __func__);

	cr1 = readl(priv->base + STM32H7_SPI_CR1);

	if (!(cr1 & STM32H7_SPI_CR1_SPE))
		return;

	/* Wait on EOT or suspend the flow */
	ret = readl_poll_timeout(priv->base + STM32H7_SPI_SR, sr,
				 !(sr & STM32H7_SPI_SR_EOT), USEC_PER_SEC);
	if (ret < 0) {
		if (cr1 & STM32H7_SPI_CR1_CSTART) {
			writel(cr1 | STM32H7_SPI_CR1_CSUSP, priv->base + STM32H7_SPI_CR1);
			if (readl_poll_timeout(priv->base + STM32H7_SPI_SR,
					       sr, !(sr & STM32H7_SPI_SR_SUSP),
					       100000) < 0)
				dev_err(dev, "Suspend request timeout\n");
		}
	}

	/* clear status flags */
	setbits_le32(priv->base + STM32H7_SPI_IFCR, STM32H7_SPI_IFCR_ALL);
}

static void stm32_spi_set_cs(struct spi_device *spi, bool en)
{
	struct stm32_spi_priv *priv = to_stm32_spi_priv(spi->master);
	int gpio = priv->cs_gpios[spi->chip_select];
	int ret = -EINVAL;

	dev_dbg(priv->master.dev, "cs=%d en=%d\n", gpio, en);

	if (gpio_is_valid(gpio))
		ret = gpio_direction_output(gpio, (spi->mode & SPI_CS_HIGH) ? en : !en);

	if (ret)
		dev_warn(priv->master.dev, "couldn't toggle cs#%u\n", spi->chip_select);
}

static void stm32_spi_set_mode(struct stm32_spi_priv *priv, unsigned mode)
{
	u32 cfg2_clrb = 0, cfg2_setb = 0;
	const struct stm32_spi_regspec *regs = priv->cfg->regs;

	dev_dbg(priv->master.dev, "mode=%d\n", mode);

	if (mode & SPI_CPOL)
		cfg2_setb |= regs->cpol.mask;
	else
		cfg2_clrb |= regs->cpol.mask;

	if (mode & SPI_CPHA)
		cfg2_setb |= regs->cpha.mask;
	else
		cfg2_clrb |= regs->cpha.mask;

	if (mode & SPI_LSB_FIRST)
		cfg2_setb |= regs->lsb_first.mask;
	else
		cfg2_clrb |= regs->lsb_first.mask;

	/* CPOL, CPHA and LSB FIRST bits have common register */
	if (cfg2_clrb || cfg2_setb)
		clrsetbits_le32(priv->base + regs->cpol.reg,
				cfg2_clrb, cfg2_setb);
}

static void stm32f4_spi_set_bpw(struct stm32_spi_priv *priv)
{
	if (priv->cur_bpw == 16)
		setbits_le32(priv->base + STM32F4_SPI_CR1, STM32F4_SPI_CR1_DFF);
	else
		clrbits_le32(priv->base + STM32F4_SPI_CR1, STM32F4_SPI_CR1_DFF);
}

static void stm32h7_spi_set_bpw(struct stm32_spi_priv *priv)
{
	clrsetbits_le32(priv->base + STM32H7_SPI_CFG1, STM32H7_SPI_CFG1_DSIZE,
			priv->cur_bpw - 1);
}

static void stm32h7_spi_set_fthlv(struct stm32_spi_priv *priv, u32 xfer_len)
{
	u32 fthlv, half_fifo;

	/* data packet should not exceed 1/2 of fifo space */
	half_fifo = (priv->fifo_size / 2);

	/* data_packet should not exceed transfer length */
	fthlv = (half_fifo > xfer_len) ? xfer_len : half_fifo;

	/* align packet size with data registers access */
	fthlv -= (fthlv % 4);

	if (!fthlv)
		fthlv = 1;
	clrsetbits_le32(priv->base + STM32H7_SPI_CFG1, STM32H7_SPI_CFG1_FTHLV,
			(fthlv - 1) << STM32H7_SPI_CFG1_FTHLV_SHIFT);
}

/**
 * stm32_spi_set_mbr - Configure baud rate divisor in master mode
 */
static void stm32_spi_set_mbr(struct stm32_spi_priv *priv, u32 mbrdiv)
{
	u32 clrb = 0, setb = 0;

	clrb |= priv->cfg->regs->br.mask;
	setb |= (mbrdiv << priv->cfg->regs->br.shift) & priv->cfg->regs->br.mask;

	clrsetbits_le32(priv->base + priv->cfg->regs->br.reg, clrb, setb);
}

static int stm32_spi_set_speed(struct stm32_spi_priv *priv, uint hz)
{
	u32 mbrdiv;
	long div;

	dev_dbg(priv->master.dev, "hz=%d\n", hz);

	if (priv->cur_hz == hz)
		return 0;

	div = DIV_ROUND_UP(priv->bus_clk_rate, hz);

	if (div < priv->cfg->baud_rate_div_min || div > priv->cfg->baud_rate_div_max)
		return -EINVAL;

	/* Determine the first power of 2 greater than or equal to div */
	if (div & (div - 1))
		mbrdiv = fls(div);
	else
		mbrdiv = fls(div) - 1;

	if (!mbrdiv)
		return -EINVAL;

	stm32_spi_set_mbr(priv, mbrdiv - 1);

	priv->cur_hz = hz;

	return 0;
}

static int stm32_spi_setup(struct spi_device *spi)
{
	struct stm32_spi_priv *priv = to_stm32_spi_priv(spi->master);
	int ret;

	stm32_spi_set_cs(spi, false);
	stm32_spi_enable(priv);

	stm32_spi_set_mode(priv, spi->mode);

	ret = stm32_spi_set_speed(priv, spi->max_speed_hz);
	if (ret)
		goto out;

	priv->cur_bpw = spi->bits_per_word;
	priv->cfg->set_bpw(priv);

	dev_dbg(priv->master.dev, "%s mode 0x%08x bits_per_word: %d speed: %d\n",
		__func__, spi->mode, spi->bits_per_word,
		spi->max_speed_hz);
out:
	stm32_spi_disable(priv);
	return ret;
}

/**
 * stm32h7_spi_number_of_data - configure number of data at current transfer
 */
static int stm32h7_spi_number_of_data(struct stm32_spi_priv *priv, u32 nb_words)
{
	if (nb_words <= STM32H7_SPI_TSIZE_MAX)
		writel(nb_words, priv->base + STM32H7_SPI_CR2);
	else
		return -EMSGSIZE;

	return 0;
}

/**
 * stm32_spi_communication_type - return transfer communication type
 * @spi_dev: pointer to the spi device
 * @transfer: pointer to spi transfer
 */
static enum spi_comm_type stm32_spi_communication_type(struct spi_device *spi_dev,
						 struct spi_transfer *transfer)
{
	enum spi_comm_type type = SPI_FULL_DUPLEX;

	if (spi_dev->mode & SPI_3WIRE) { /* MISO/MOSI signals shared */
		/*
		 * SPI_3WIRE and xfer->tx_buf != NULL and xfer->rx_buf != NULL
		 * is forbidden and unvalidated by SPI subsystem so depending
		 * on the valid buffer, we can determine the direction of the
		 * transfer.
		 */
		if (!transfer->tx_buf)
			type = SPI_3WIRE_RX;
		else
			type = SPI_3WIRE_TX;
	} else {
		if (!transfer->tx_buf)
			type = SPI_SIMPLEX_RX;
		else if (!transfer->rx_buf)
			type = SPI_SIMPLEX_TX;
	}

	return type;
}

static int stm32_spi_transfer_one(struct spi_device *spi_dev,
				  struct spi_transfer *t)
{
	struct stm32_spi_priv *priv = to_stm32_spi_priv(spi_dev->master);
	enum spi_comm_type mode;
	int ret;

	priv->tx_buf = t->tx_buf;
	priv->rx_buf = t->rx_buf;
	priv->tx_len = priv->tx_buf ? t->len : 0;
	priv->rx_len = priv->rx_buf ? t->len : 0;

	if (priv->cfg->set_number_of_data) {
		ret = priv->cfg->set_number_of_data(priv, t->len);
		if (ret < 0)
			return ret;
	}

	mode = stm32_spi_communication_type(spi_dev, t);

	if (priv->cur_xferlen != t->len || priv->cur_mode != mode) {
		priv->cur_mode = mode;
		priv->cur_xferlen = t->len;

		/* Disable the SPI hardware to unlock CFG1/CFG2 registers */
		stm32_spi_disable(priv);

		ret = priv->cfg->set_mode(priv, mode);
		if (ret)
			return ret;

		/* Enable the SPI hardware */
		stm32_spi_enable(priv);
	}

	dev_dbg(priv->master.dev, "priv->tx_len=%d priv->rx_len=%d\n",
		priv->tx_len, priv->rx_len);

	ret = priv->cfg->transfer_one(priv, t);

	priv->cfg->stop_transfer(priv);

	return ret;
}

/**
 * stm32f4_spi_set_mode - configure communication mode
 * @spi: pointer to the spi controller data structure
 * @comm_type: type of communication to configure
 */
static int stm32f4_spi_set_mode(struct stm32_spi_priv *priv, enum spi_comm_type comm_type)
{

	if (comm_type == SPI_3WIRE_TX || comm_type == SPI_SIMPLEX_TX) {
		setbits_le32(priv->base + STM32F4_SPI_CR1,
			     STM32F4_SPI_CR1_BIDIMODE |
			     STM32F4_SPI_CR1_BIDIOE);
	} else if (comm_type == SPI_FULL_DUPLEX ||
		   comm_type == SPI_SIMPLEX_RX) {
		clrbits_le32(priv->base + STM32F4_SPI_CR1,
			     STM32F4_SPI_CR1_BIDIMODE |
			     STM32F4_SPI_CR1_BIDIOE);
	} else if (comm_type == SPI_3WIRE_RX) {
		setbits_le32(priv->base + STM32F4_SPI_CR1,
					STM32F4_SPI_CR1_BIDIMODE);
		clrbits_le32(priv->base + STM32F4_SPI_CR1,
					STM32F4_SPI_CR1_BIDIOE);
	} else {
		return -EINVAL;
	}

	return 0;
}

static int stm32f4_spi_transfer_one(struct stm32_spi_priv *priv,
				    struct spi_transfer *t)
{
	struct device_d *dev = priv->master.dev;
	u64 start;

	/* We need to shift out some dummy data to drive the clock */
	if (!priv->tx_len)
		priv->tx_len = t->len;

	start = get_time_ns();

	readl(priv->base + STM32F4_SPI_DR);

	stm32f4_spi_write_tx(priv);

	/* no time for pollers; single-word FIFO */
	while (!is_timeout_non_interruptible(start, SECOND)) {
		u32 sr;

		sr = readl(priv->base + STM32F4_SPI_SR);

		if (priv->cur_mode == SPI_SIMPLEX_TX ||
		    priv->cur_mode == SPI_3WIRE_TX) {
			/* OVR flag shouldn't be handled for TX only mode */
			sr &= ~(STM32F4_SPI_SR_OVR | STM32F4_SPI_SR_RXNE);
		}

		if (priv->cur_mode == SPI_FULL_DUPLEX ||
		    priv->cur_mode == SPI_SIMPLEX_RX || priv->cur_mode == SPI_3WIRE_RX) {
			/* TXE flag is set and is handled when RXNE flag occurs */
			sr &= ~STM32F4_SPI_SR_TXE;
		}

		if (sr & STM32F4_SPI_SR_OVR) {
			dev_warn(dev, "Overrun: received value discarded (sr=%x, cur_mode=%u)\n",
				 sr, priv->cur_mode);

			/* Sequence to clear OVR flag */
			readl(priv->base + STM32F4_SPI_DR);
			readl(priv->base + STM32F4_SPI_SR);

			return -EIO;
		}

		if (sr & STM32F4_SPI_SR_TXE) {
			if (priv->tx_len)
				stm32f4_spi_write_tx(priv);
		}

		if (sr & STM32F4_SPI_SR_RXNE) {
			stm32f4_spi_read_rx(priv);
			if (priv->tx_len)/* Load data for discontinuous mode */
				stm32f4_spi_write_tx(priv);
		}

		if (!priv->tx_len && !priv->rx_len) {
			dev_dbg(dev, "!BUSY\n");
			return 0;
		}
	}

	return -ETIMEDOUT;
}

/**
 * stm32h7_spi_set_mode - configure communication mode
 * @spi: pointer to the spi controller data structure
 * @comm_type: type of communication to configure
 */
static int stm32h7_spi_set_mode(struct stm32_spi_priv *priv,
				enum spi_comm_type comm_type)
{
	u32 mode;

	if (comm_type == SPI_3WIRE_RX) {
		mode = STM32H7_SPI_HALF_DUPLEX;
		clrbits_le32(priv->base + STM32H7_SPI_CR1, STM32H7_SPI_CR1_HDDIR);
	} else if (comm_type == SPI_3WIRE_TX) {
		mode = STM32H7_SPI_HALF_DUPLEX;
		setbits_le32(priv->base + STM32H7_SPI_CR1, STM32H7_SPI_CR1_HDDIR);
	} else if (comm_type == SPI_SIMPLEX_RX) {
		mode = STM32H7_SPI_SIMPLEX_RX;
	} else if (comm_type == SPI_SIMPLEX_TX) {
		mode = STM32H7_SPI_SIMPLEX_TX;
	} else {
		mode = STM32H7_SPI_FULL_DUPLEX;
	}

	clrsetbits_le32(priv->base + STM32H7_SPI_CFG2, STM32H7_SPI_CFG2_COMM,
			mode << STM32H7_SPI_CFG2_COMM_SHIFT);

	stm32h7_spi_set_fthlv(priv, priv->cur_xferlen);

	return 0;
}

static int stm32h7_spi_transfer_one(struct stm32_spi_priv *priv,
				    struct spi_transfer *t)
{
	struct device_d *dev = priv->master.dev;
	u32 sr;
	u32 ifcr = 0;
	int xfer_status = 0;

	/* Be sure to have data in fifo before starting data transfer */
	if (priv->tx_buf)
		stm32h7_spi_write_txfifo(priv);

	setbits_le32(priv->base + STM32H7_SPI_CR1, STM32H7_SPI_CR1_CSTART);

	while (1) {
		sr = readl(priv->base + STM32H7_SPI_SR);

		if (sr & STM32H7_SPI_SR_OVR) {
			dev_err(dev, "Overrun: RX data lost\n");
			xfer_status = -EIO;
			break;
		}

		if (sr & STM32H7_SPI_SR_SUSP) {
			dev_warn(dev, "System too slow is limiting data throughput\n");

			if (priv->rx_buf && priv->rx_len > 0)
				stm32h7_spi_read_rxfifo(priv);

			ifcr |= STM32H7_SPI_SR_SUSP;
		}

		if (sr & STM32H7_SPI_SR_TXTF)
			ifcr |= STM32H7_SPI_SR_TXTF;

		if (sr & STM32H7_SPI_SR_TXP)
			if (priv->tx_buf && priv->tx_len > 0)
				stm32h7_spi_write_txfifo(priv);

		if (sr & STM32H7_SPI_SR_RXP)
			if (priv->rx_buf && priv->rx_len > 0)
				stm32h7_spi_read_rxfifo(priv);

		if (sr & STM32H7_SPI_SR_EOT) {
			if (priv->rx_buf && priv->rx_len > 0)
				stm32h7_spi_read_rxfifo(priv);
			break;
		}

		writel(ifcr, priv->base + STM32H7_SPI_IFCR);
	}

	/* clear status flags */
	setbits_le32(priv->base + STM32H7_SPI_IFCR, STM32H7_SPI_IFCR_ALL);

	return xfer_status;
}

static int stm32_spi_transfer(struct spi_device *spi, struct spi_message *mesg)
{
	struct stm32_spi_priv *priv = to_stm32_spi_priv(spi->master);
	struct spi_transfer *t;
	unsigned int cs_change;
	const int nsecs = 50;
	int ret = 0;

	stm32_spi_enable(priv);

	stm32_spi_set_cs(spi, true);

	cs_change = 0;

	mesg->actual_length = 0;

	list_for_each_entry(t, &mesg->transfers, transfer_list) {
		if (cs_change) {
			ndelay(nsecs);
			stm32_spi_set_cs(spi, false);
			ndelay(nsecs);
			stm32_spi_set_cs(spi, true);
		}

		cs_change = t->cs_change;

		ret = stm32_spi_transfer_one(spi, t);
		if (ret)
			goto out;

		mesg->actual_length += t->len;

		if (cs_change)
			stm32_spi_set_cs(spi, true);
	}

	if (!cs_change)
		stm32_spi_set_cs(spi, false);

out:
	stm32_spi_disable(priv);
	return ret;
}

static int stm32h7_spi_get_fifo_size(struct stm32_spi_priv *priv)
{
	u32 count = 0;

	stm32_spi_enable(priv);

	while (readl(priv->base + STM32H7_SPI_SR) & STM32H7_SPI_SR_TXP)
		writeb(++count, priv->base + STM32H7_SPI_TXDR);

	stm32_spi_disable(priv);

	dev_dbg(priv->master.dev, "%d x 8-bit fifo size\n", count);

	return count;
}

static void stm32_spi_dt_probe(struct stm32_spi_priv *priv)
{
	struct device_node *node = priv->master.dev->device_node;
	int i;

	priv->master.num_chipselect = of_gpio_named_count(node, "cs-gpios");
	priv->cs_gpios = xzalloc(sizeof(u32) * priv->master.num_chipselect);

	for (i = 0; i < priv->master.num_chipselect; i++)
		priv->cs_gpios[i] = of_get_named_gpio(node, "cs-gpios", i);
}

/**
 * stm32f4_spi_config - Configure SPI controller as SPI master
 */
static void stm32f4_spi_config(struct stm32_spi_priv *priv)
{
	/* Ensure I2SMOD bit is kept cleared */
	clrbits_le32(priv->base + STM32F4_SPI_I2SCFGR, STM32F4_SPI_I2SCFGR_I2SMOD);

	/*
	 * - SS input value high
	 * - transmitter half duplex direction
	 * - Set the master mode (default Motorola mode)
	 * - Consider 1 master/n slaves configuration and
	 *   SS input value is determined by the SSI bit
	 */
	setbits_le32(priv->base + STM32F4_SPI_CR1, STM32F4_SPI_CR1_SSI |
						 STM32F4_SPI_CR1_BIDIOE |
						 STM32F4_SPI_CR1_MSTR |
						 STM32F4_SPI_CR1_SSM);
}

/**
 * stm32h7_spi_config - Configure SPI controller as SPI master
 */
static void stm32h7_spi_config(struct stm32_spi_priv *priv)
{
	/* Ensure I2SMOD bit is kept cleared */
	clrbits_le32(priv->base + STM32H7_SPI_I2SCFGR, STM32H7_SPI_I2SCFGR_I2SMOD);

	/*
	 * - SS input value high
	 * - transmitter half duplex direction
	 * - automatic communication suspend when RX-Fifo is full
	 */
	setbits_le32(priv->base + STM32H7_SPI_CR1,
		     STM32H7_SPI_CR1_SSI | STM32H7_SPI_CR1_HDDIR | STM32H7_SPI_CR1_MASRX);

	/*
	 * - Set the master mode (default Motorola mode)
	 * - Consider 1 master/n slaves configuration and
	 *   SS input value is determined by the SSI bit
	 * - keep control of all associated GPIOs
	 */
	setbits_le32(priv->base + STM32H7_SPI_CFG2,
		     STM32H7_SPI_CFG2_MASTER | STM32H7_SPI_CFG2_SSM | STM32H7_SPI_CFG2_AFCNTR);
}

static int stm32h7_spi_adjust_op_size(struct spi_mem *mem, struct spi_mem_op *op)
{
	if (op->data.nbytes > STM32H7_SPI_TSIZE_MAX)
		op->data.nbytes = STM32H7_SPI_TSIZE_MAX;

	return 0;
}

static int stm32_spi_exec_op(struct spi_mem *mem, const struct spi_mem_op *op)
{
	return -ENOTSUPP;
}

static const struct spi_controller_mem_ops stm32h7_spi_mem_ops = {
	.adjust_op_size = stm32h7_spi_adjust_op_size,
	.exec_op = stm32_spi_exec_op,
};

static int stm32_spi_probe(struct device_d *dev)
{
	struct resource *iores;
	struct spi_master *master;
	struct stm32_spi_priv *priv;
	int ret;

	iores = dev_request_mem_resource(dev, 0);
	if (IS_ERR(iores))
		return PTR_ERR(iores);

	priv = dev->priv = xzalloc(sizeof(*priv));

	priv->base = IOMEM(iores->start);
	priv->cfg = device_get_match_data(dev);

	master = &priv->master;
	master->dev = dev;

	master->setup = stm32_spi_setup;
	master->transfer = stm32_spi_transfer;
	master->mem_ops = priv->cfg->mem_ops;

	master->bus_num = -1;
	stm32_spi_dt_probe(priv);

	priv->clk = clk_get(dev, NULL);
	if (IS_ERR(priv->clk))
		return PTR_ERR(priv->clk);

	ret = clk_enable(priv->clk);
	if (ret)
		return ret;

	priv->bus_clk_rate = clk_get_rate(priv->clk);

	ret = device_reset_us(dev, 2);
	if (ret)
		return ret;

	if (priv->cfg->has_fifo)
		priv->fifo_size = priv->cfg->get_fifo_size(priv);

	priv->cfg->config(priv);

	priv->cur_mode = STM32H7_SPI_FULL_DUPLEX;
	priv->cur_xferlen = 0;

	return spi_register_master(master);
}

static void stm32_spi_remove(struct device_d *dev)
{
	struct stm32_spi_priv *priv = dev->priv;

	priv->cfg->stop_transfer(priv);
	stm32_spi_disable(priv);
};

static const struct stm32_spi_cfg stm32f4_spi_cfg = {
	.regs = &stm32f4_spi_regspec,
	.stop_transfer = stm32f4_spi_stop_transfer,
	.config = stm32f4_spi_config,
	.set_bpw = stm32f4_spi_set_bpw,
	.set_mode = stm32f4_spi_set_mode,
	.transfer_one = stm32f4_spi_transfer_one,
	.baud_rate_div_min = STM32F4_SPI_BR_DIV_MIN,
	.baud_rate_div_max = STM32F4_SPI_BR_DIV_MAX,
	.has_fifo = false,
};

static const struct stm32_spi_cfg stm32h7_spi_cfg = {
	.regs = &stm32h7_spi_regspec,
	.stop_transfer = stm32h7_spi_stop_transfer,
	.get_fifo_size = stm32h7_spi_get_fifo_size,
	.config = stm32h7_spi_config,
	.set_bpw = stm32h7_spi_set_bpw,
	.set_mode = stm32h7_spi_set_mode,
	.set_number_of_data = stm32h7_spi_number_of_data,
	.mem_ops = &stm32h7_spi_mem_ops,
	.transfer_one = stm32h7_spi_transfer_one,
	.baud_rate_div_min = STM32H7_MBR_DIV_MIN,
	.baud_rate_div_max = STM32H7_MBR_DIV_MAX,
	.has_fifo = true,
};

static const struct of_device_id stm32_spi_ids[] = {
	{ .compatible = "st,stm32f4-spi", .data = &stm32f4_spi_cfg },
	{ .compatible = "st,stm32h7-spi", .data = &stm32h7_spi_cfg },
	{ /* sentinel */ }
};

static struct driver_d stm32_spi_driver = {
	.name  = "stm32_spi",
	.probe = stm32_spi_probe,
	.remove = stm32_spi_remove,
	.of_compatible = stm32_spi_ids,
};
coredevice_platform_driver(stm32_spi_driver);

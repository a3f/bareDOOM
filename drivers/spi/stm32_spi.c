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
#include <spi/spi.h>
#include <linux/bitops.h>
#include <clock.h>
#include <gpio.h>
#include <of_gpio.h>
#include <linux/bitfield.h>
#include <linux/iopoll.h>

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

/* STM32H7_SPI_CFG1 bit fields */
#define STM32H7_SPI_CFG1_DSIZE		GENMASK(4, 0)
#define STM32H7_SPI_CFG1_DSIZE_MIN	3
#define STM32H7_SPI_CFG1_FTHLV_SHIFT	5
#define STM32H7_SPI_CFG1_FTHLV		GENMASK(8, 5)
#define STM32H7_SPI_CFG1_MBR_SHIFT	28
#define STM32H7_SPI_CFG1_MBR		GENMASK(30, 28)
#define STM32H7_SPI_CFG1_MBR_MIN	0
#define STM32H7_SPI_CFG1_MBR_MAX	FIELD_GET(STM32H7_SPI_CFG1_MBR, STM32H7_SPI_CFG1_MBR)

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
	u32			cur_mode;
};

static inline struct stm32_spi_priv *to_stm32_spi_priv(struct spi_master *master)
{
	return container_of(master, struct stm32_spi_priv, master);
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
	setbits_le32(priv->base + STM32H7_SPI_CR1, STM32H7_SPI_CR1_SPE);
}

static void stm32_spi_disable(struct stm32_spi_priv *priv)
{
	clrbits_le32(priv->base + STM32H7_SPI_CR1, STM32H7_SPI_CR1_SPE);
}

static void stm32_spi_stopxfer(struct stm32_spi_priv *priv)
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

	dev_dbg(priv->master.dev, "mode=%d\n", mode);

	if (mode & SPI_CPOL)
		cfg2_setb |= STM32H7_SPI_CFG2_CPOL;
	else
		cfg2_clrb |= STM32H7_SPI_CFG2_CPOL;

	if (mode & SPI_CPHA)
		cfg2_setb |= STM32H7_SPI_CFG2_CPHA;
	else
		cfg2_clrb |= STM32H7_SPI_CFG2_CPHA;

	if (mode & SPI_LSB_FIRST)
		cfg2_setb |= STM32H7_SPI_CFG2_LSBFRST;
	else
		cfg2_clrb |= STM32H7_SPI_CFG2_LSBFRST;

	if (cfg2_clrb || cfg2_setb)
		clrsetbits_le32(priv->base + STM32H7_SPI_CFG2,
				cfg2_clrb, cfg2_setb);
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

static int stm32_spi_set_speed(struct stm32_spi_priv *priv, uint hz)
{
	u32 mbrdiv;
	long div;

	dev_dbg(priv->master.dev, "hz=%d\n", hz);

	if (priv->cur_hz == hz)
		return 0;

	div = DIV_ROUND_UP(priv->bus_clk_rate, hz);

	if (div < STM32H7_MBR_DIV_MIN || div > STM32H7_MBR_DIV_MAX)
		return -EINVAL;

	/* Determine the first power of 2 greater than or equal to div */
	if (div & (div - 1))
		mbrdiv = fls(div);
	else
		mbrdiv = fls(div) - 1;

	if (!mbrdiv)
		return -EINVAL;

	clrsetbits_le32(priv->base + STM32H7_SPI_CFG1, STM32H7_SPI_CFG1_MBR,
			(mbrdiv - 1) << STM32H7_SPI_CFG1_MBR_SHIFT);

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
	clrsetbits_le32(priv->base + STM32H7_SPI_CFG1, STM32H7_SPI_CFG1_DSIZE,
			priv->cur_bpw - 1);

	dev_dbg(priv->master.dev, "%s mode 0x%08x bits_per_word: %d speed: %d\n",
		__func__, spi->mode, spi->bits_per_word,
		spi->max_speed_hz);
out:
	stm32_spi_disable(priv);
	return ret;
}

static int stm32_spi_transfer_one(struct stm32_spi_priv *priv,
				  struct spi_transfer *t)
{
	struct device_d *dev = priv->master.dev;
	u32 sr;
	u32 ifcr = 0;
	u32 mode;
	int xfer_status = 0;

	if (t->len <= STM32H7_SPI_CR2_TSIZE)
		writel(t->len, priv->base + STM32H7_SPI_CR2);
	else
		return -EMSGSIZE;

	priv->tx_buf = t->tx_buf;
	priv->rx_buf = t->rx_buf;
	priv->tx_len = priv->tx_buf ? t->len : 0;
	priv->rx_len = priv->rx_buf ? t->len : 0;

	mode = STM32H7_SPI_FULL_DUPLEX;
	if (!priv->tx_buf)
		mode = STM32H7_SPI_SIMPLEX_RX;
	else if (!priv->rx_buf)
		mode = STM32H7_SPI_SIMPLEX_TX;

	if (priv->cur_xferlen != t->len || priv->cur_mode != mode) {
		priv->cur_mode = mode;
		priv->cur_xferlen = t->len;

		/* Disable the SPI hardware to unlock CFG1/CFG2 registers */
		stm32_spi_disable(priv);

		clrsetbits_le32(priv->base + STM32H7_SPI_CFG2, STM32H7_SPI_CFG2_COMM,
				mode << STM32H7_SPI_CFG2_COMM_SHIFT);

		stm32h7_spi_set_fthlv(priv, t->len);

		/* Enable the SPI hardware */
		stm32_spi_enable(priv);
	}

	dev_dbg(dev, "priv->tx_len=%d priv->rx_len=%d\n",
		priv->tx_len, priv->rx_len);

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
	stm32_spi_stopxfer(priv);

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

		ret = stm32_spi_transfer_one(priv, t);
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

	master = &priv->master;
	master->dev = dev;

	master->setup = stm32_spi_setup;
	master->transfer = stm32_spi_transfer;

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

	priv->fifo_size = stm32h7_spi_get_fifo_size(priv);

	priv->cur_mode = STM32H7_SPI_FULL_DUPLEX;
	priv->cur_xferlen = 0;

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

	return spi_register_master(master);
}

static void stm32_spi_remove(struct device_d *dev)
{
	struct stm32_spi_priv *priv = dev->priv;

	stm32_spi_stopxfer(priv);
	stm32_spi_disable(priv);
};

static const struct of_device_id stm32_spi_ids[] = {
	{ .compatible = "st,stm32h7-spi", },
	{ /* sentinel */ }
};

static struct driver_d stm32_spi_driver = {
	.name  = "stm32_spi",
	.probe = stm32_spi_probe,
	.remove = stm32_spi_remove,
	.of_compatible = stm32_spi_ids,
};
coredevice_platform_driver(stm32_spi_driver);

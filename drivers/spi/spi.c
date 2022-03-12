// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) 2008 Sascha Hauer, Pengutronix
 *
 * Derived from Linux SPI Framework
 *
 * Copyright (C) 2005 David Brownell
 */

#include <common.h>
#include <linux/spi/spi-mem.h>
#include <spi/spi.h>
#include <xfuncs.h>
#include <malloc.h>
#include <errno.h>
#include <init.h>
#include <of.h>

/* SPI devices should normally not be created by SPI device drivers; that
 * would make them board-specific.  Similarly with SPI master drivers.
 * Device registration normally goes into like arch/.../mach.../board-YYY.c
 * with other readonly (flashable) information about mainboard devices.
 */

struct boardinfo {
	struct list_head	list;
	unsigned		n_board_info;
	struct spi_board_info	board_info[0];
};

static LIST_HEAD(board_list);

/**
 * spi_new_device - instantiate one new SPI device
 * @master: Controller to which device is connected
 * @chip: Describes the SPI device
 * Context: can sleep
 *
 * On typical mainboards, this is purely internal; and it's not needed
 * after board init creates the hard-wired devices.  Some development
 * platforms may not be able to use spi_register_board_info though, and
 * this is exported so that for example a USB or parport based adapter
 * driver could add devices (which it would learn about out-of-band).
 *
 * Returns the new device, or NULL.
 */
struct spi_device *spi_new_device(struct spi_controller *ctrl,
				  struct spi_board_info *chip)
{
	struct spi_device	*proxy;
	struct spi_mem		*mem;
	int			status;

	/* Chipselects are numbered 0..max; validate. */
	if (chip->chip_select >= ctrl->num_chipselect) {
		debug("cs%d > max %d\n",
			chip->chip_select,
			ctrl->num_chipselect);
		return NULL;
	}

	proxy = xzalloc(sizeof *proxy);
	proxy->master = ctrl;
	proxy->chip_select = chip->chip_select;
	proxy->max_speed_hz = chip->max_speed_hz;
	proxy->mode = chip->mode;
	proxy->bits_per_word = chip->bits_per_word ? chip->bits_per_word : 8;
	proxy->dev.platform_data = chip->platform_data;
	proxy->dev.bus = &spi_bus;
	dev_set_name(&proxy->dev, chip->name);
	/* allocate a free id for this chip */
	proxy->dev.id = DEVICE_ID_DYNAMIC;
	proxy->dev.type_data = proxy;
	proxy->dev.device_node = chip->device_node;
	proxy->dev.parent = ctrl->dev;
	proxy->master = proxy->controller = ctrl;

	mem = xzalloc(sizeof *mem);
	mem->spi = proxy;

	if (ctrl->mem_ops && ctrl->mem_ops->get_name)
		mem->name = ctrl->mem_ops->get_name(mem);
	else
		mem->name = dev_name(&proxy->dev);
	proxy->mem = mem;

	/* drivers may modify this initial i/o setup */
	status = ctrl->setup(proxy);
	if (status < 0) {
		printf("can't setup %s, status %d\n",
				proxy->dev.name, status);
		goto fail;
	}

	status = register_device(&proxy->dev);
	if (status)
		goto fail;

	chip->device_node->dev = &proxy->dev;

	return proxy;
fail:
	free(proxy);
	return NULL;
}
EXPORT_SYMBOL(spi_new_device);

static void spi_of_register_slaves(struct spi_controller *ctrl)
{
	struct device_node *n;
	struct spi_board_info chip;
	struct property *reg;
	struct device_node *node = ctrl->dev->device_node;

	if (!IS_ENABLED(CONFIG_OFDEVICE))
		return;

	if (!node)
		return;

	for_each_available_child_of_node(node, n) {
		memset(&chip, 0, sizeof(chip));
		chip.name = xstrdup(n->name);
		chip.bus_num = ctrl->bus_num;
		/* Mode (clock phase/polarity/etc.) */
		if (of_property_read_bool(n, "spi-cpha"))
			chip.mode |= SPI_CPHA;
		if (of_property_read_bool(n, "spi-cpol"))
			chip.mode |= SPI_CPOL;
		if (of_property_read_bool(n, "spi-cs-high"))
			chip.mode |= SPI_CS_HIGH;
		if (of_property_read_bool(n, "spi-3wire"))
			chip.mode |= SPI_3WIRE;
		of_property_read_u32(n, "spi-max-frequency",
				&chip.max_speed_hz);
		reg = of_find_property(n, "reg", NULL);
		if (!reg)
			continue;
		chip.chip_select = of_read_number(reg->value, 1);
		chip.device_node = n;
		spi_register_board_info(&chip, 1);
	}
}

/**
 * spi_register_board_info - register SPI devices for a given board
 * @info: array of chip descriptors
 * @n: how many descriptors are provided
 * Context: can sleep
 *
 * Board-specific early init code calls this (probably during arch_initcall)
 * with segments of the SPI device table.  Any device nodes are created later,
 * after the relevant parent SPI controller (bus_num) is defined.  We keep
 * this table of devices forever, so that reloading a controller driver will
 * not make Linux forget about these hard-wired devices.
 *
 * Other code can also call this, e.g. a particular add-on board might provide
 * SPI devices through its expansion connector, so code initializing that board
 * would naturally declare its SPI devices.
 *
 * The board info passed can safely be __initdata ... but be careful of
 * any embedded pointers (platform_data, etc), they're copied as-is.
 */
int
spi_register_board_info(struct spi_board_info const *info, int n)
{
	struct boardinfo	*bi;

	bi = xmalloc(sizeof(*bi) + n * sizeof *info);

	bi->n_board_info = n;
	memcpy(bi->board_info, info, n * sizeof *info);

	list_add_tail(&bi->list, &board_list);

	return 0;
}

static void scan_boardinfo(struct spi_controller *ctrl)
{
	struct boardinfo	*bi;

	list_for_each_entry(bi, &board_list, list) {
		struct spi_board_info	*chip = bi->board_info;
		unsigned		n;

		for (n = bi->n_board_info; n > 0; n--, chip++) {
			debug("%s %d %d\n", __FUNCTION__, chip->bus_num, ctrl->bus_num);
			if (chip->bus_num != ctrl->bus_num)
				continue;
			/* NOTE: this relies on spi_new_device to
			 * issue diagnostics when given bogus inputs
			 */
			(void) spi_new_device(ctrl, chip);
		}
	}
}

static LIST_HEAD(spi_controller_list);

static int spi_controller_check_ops(struct spi_controller *ctlr)
{
	/*
	 * The controller may implement only the high-level SPI-memory like
	 * operations if it does not support regular SPI transfers, and this is
	 * valid use case.
	 * If ->mem_ops is NULL, we request that at least one of the
	 * ->transfer_xxx() method be implemented.
	 */
	if (ctlr->mem_ops) {
		if (!ctlr->mem_ops->exec_op)
			return -EINVAL;
	} else if (!ctlr->transfer) {
		return -EINVAL;
	}

	return 0;
}


/**
 * spi_register_ctrl - register SPI ctrl controller
 * @ctrl: initialized ctrl, originally from spi_alloc_ctrl()
 * Context: can sleep
 *
 * SPI controllers connect to their drivers using some non-SPI bus,
 * such as the platform bus.  The final stage of probe() in that code
 * includes calling spi_register_ctrl() to hook up to this SPI bus glue.
 *
 * SPI controllers use board specific (often SOC specific) bus numbers,
 * and board-specific addressing for SPI devices combines those numbers
 * with chip select numbers.  Since SPI does not directly support dynamic
 * device identification, boards need configuration tables telling which
 * chip is at which address.
 *
 * This must be called from context that can sleep.  It returns zero on
 * success, else a negative error code (dropping the ctrl's refcount).
 * After a successful return, the caller is responsible for calling
 * spi_unregister_ctrl().
 */
int spi_register_controller(struct spi_controller *ctrl)
{
	static int dyn_bus_id = (1 << 15) - 1;
	int			status = -ENODEV;

	debug("%s: %s:%d\n", __func__, ctrl->dev->name, ctrl->dev->id);

	/*
	 * Make sure all necessary hooks are implemented before registering
	 * the SPI controller.
	 */
	status = spi_controller_check_ops(ctrl);
	if (status)
		return status;

	/* even if it's just one always-selected device, there must
	 * be at least one chipselect
	 */
	if (ctrl->num_chipselect == 0)
		return -EINVAL;

	if ((ctrl->bus_num < 0) && ctrl->dev->device_node)
		ctrl->bus_num = of_alias_get_id(ctrl->dev->device_node, "spi");

	/* convention:  dynamically assigned bus IDs count down from the max */
	if (ctrl->bus_num < 0)
		ctrl->bus_num = dyn_bus_id--;

	list_add_tail(&ctrl->list, &spi_controller_list);

	spi_of_register_slaves(ctrl);

	/* populate children from any spi device tables */
	scan_boardinfo(ctrl);
	status = 0;

	return status;
}
EXPORT_SYMBOL(spi_register_controller);

struct spi_controller *spi_get_controller(int bus)
{
	struct spi_controller* m;

	list_for_each_entry(m, &spi_controller_list, list) {
		if (m->bus_num == bus)
			return m;
	}

	return NULL;
}

static int __spi_validate(struct spi_device *spi, struct spi_message *message)
{
	struct spi_controller *ctlr = spi->controller;
	struct spi_transfer *xfer;
	int w_size;

	if (list_empty(&message->transfers))
		return -EINVAL;

	list_for_each_entry(xfer, &message->transfers, transfer_list) {
		if (!xfer->bits_per_word)
			xfer->bits_per_word = spi->bits_per_word;

		if (!xfer->speed_hz)
			xfer->speed_hz = spi->max_speed_hz;

		if (ctlr->max_speed_hz && xfer->speed_hz > ctlr->max_speed_hz)
			xfer->speed_hz = ctlr->max_speed_hz;

		/* Half-duplex links include original MicroWire, and ones with
		 * only one data pin like SPI_3WIRE (switches direction) or where
		 * either MOSI or MISO is missing.  They can also be caused by
		 * software limitations.
		 */
		if ((spi->mode & SPI_3WIRE) && xfer->rx_buf && xfer->tx_buf)
			return -EINVAL;

		/*
		 * SPI transfer length should be multiple of SPI word size
		 * where SPI word size should be power-of-two multiple
		 */
		if (xfer->bits_per_word <= 8)
			w_size = 1;
		else if (xfer->bits_per_word <= 16)
			w_size = 2;
		else
			w_size = 4;

		/* No partial transfers accepted */
		if (xfer->len % w_size)
			return -EINVAL;
	}

	message->status = -EINPROGRESS;

	return 0;
}

int spi_sync(struct spi_device *spi, struct spi_message *message)
{
	int status;

	status = __spi_validate(spi, message);
	if (status != 0)
		return status;

	return spi->controller->transfer(spi, message);
}

/**
 * spi_write_then_read - SPI synchronous write followed by read
 * @spi: device with which data will be exchanged
 * @txbuf: data to be written
 * @n_tx: size of txbuf, in bytes
 * @rxbuf: buffer into which data will be read
 * @n_rx: size of rxbuf, in bytes
 * Context: can sleep
 *
 * This performs a half duplex MicroWire style transaction with the
 * device, sending txbuf and then reading rxbuf.  The return value
 * is zero for success, else a negative errno status code.
 * This call may only be used from a context that may sleep.
 */
int spi_write_then_read(struct spi_device *spi,
		const void *txbuf, unsigned n_tx,
		void *rxbuf, unsigned n_rx)
{
	int			status;
	struct spi_message	message;
	struct spi_transfer	x[2];

	spi_message_init(&message);
	memset(x, 0, sizeof x);
	if (n_tx) {
		x[0].len = n_tx;
		spi_message_add_tail(&x[0], &message);
	}
	if (n_rx) {
		x[1].len = n_rx;
		spi_message_add_tail(&x[1], &message);
	}

	x[0].tx_buf = txbuf;
	x[1].rx_buf = rxbuf;

	/* do the i/o */
	status = spi_sync(spi, &message);
	return status;
}
EXPORT_SYMBOL(spi_write_then_read);

static int spi_probe(struct device_d *dev)
{
	return dev->driver->probe(dev);
}

static void spi_remove(struct device_d *dev)
{
	if (dev->driver->remove)
		dev->driver->remove(dev);
}

struct bus_type spi_bus = {
	.name = "spi",
	.match = device_match_of_modalias,
	.probe = spi_probe,
	.remove = spi_remove,
};

static int spi_bus_init(void)
{
	return bus_register(&spi_bus);
}
pure_initcall(spi_bus_init);

// SPDX-License-Identifier: GPL-2.0-only
#define pr_fmt(fmt)	"gpiolib: " fmt

#include <init.h>
#include <common.h>
#include <command.h>
#include <complete.h>
#include <gpio.h>
#include <of_gpio.h>
#include <gpiod.h>
#include <errno.h>
#include <malloc.h>

static LIST_HEAD(chip_list);

struct gpio_info {
	struct gpio_chip *chip;
	bool requested;
	bool active_low;
	char *label;
	char *name;
};

static struct gpio_info *gpio_desc;

static int gpio_desc_alloc(void)
{
	gpio_desc = xzalloc(sizeof(struct gpio_info) * ARCH_NR_GPIOS);

	return 0;
}
pure_initcall(gpio_desc_alloc);

static int gpio_ensure_requested(struct gpio_info *gi, int gpio)
{
	if (gi->requested)
		return 0;

	return gpio_request(gpio, "gpio");
}

static struct gpio_info *gpio_to_desc(unsigned gpio)
{
	if (gpio_is_valid(gpio))
		if (gpio_desc[gpio].chip)
			return &gpio_desc[gpio];

	pr_warning("invalid GPIO %d\n", gpio);

	return NULL;
}

static unsigned gpioinfo_chip_offset(struct gpio_info *gi)
{
	return (gi - gpio_desc) - gi->chip->base;
}

static int gpio_adjust_value(struct gpio_info *gi,
			     int value)
{
	if (value < 0)
		return value;

	return !!value ^ gi->active_low;
}

static int gpioinfo_request(struct gpio_info *gi, const char *label)
{
	int ret;

	if (gi->requested) {
		ret = -EBUSY;
		goto done;
	}

	ret = 0;

	if (gi->chip->ops->request) {
		ret = gi->chip->ops->request(gi->chip,
					     gpioinfo_chip_offset(gi));
		if (ret)
			goto done;
	}

	gi->requested = true;
	gi->active_low = false;
	gi->label = xstrdup(label);

done:
	if (ret)
		pr_err("_gpio_request: gpio-%td (%s) status %d\n",
		       gi - gpio_desc, label ? : "?", ret);

	return ret;
}

int gpio_find_by_label(const char *label)
{
	int i;

	for (i = 0; i < ARCH_NR_GPIOS; i++) {
		struct gpio_info *info = &gpio_desc[i];

		if (!info->requested || !info->chip || !info->label)
			continue;

		if (!strcmp(info->label, label))
			return i;
	}

	return -ENOENT;
}

int gpio_find_by_name(const char *name)
{
	int i;

	for (i = 0; i < ARCH_NR_GPIOS; i++) {
		struct gpio_info *info = &gpio_desc[i];

		if (!info->chip || !info->name)
			continue;

		if (!strcmp(info->name, name))
			return i;
	}

	return -ENOENT;
}

int gpio_request(unsigned gpio, const char *label)
{
	struct gpio_info *gi = gpio_to_desc(gpio);

	if (!gi) {
		pr_err("_gpio_request: gpio-%d (%s) status %d\n",
			 gpio, label ? : "?", -ENODEV);
		return -ENODEV;
	}

	return gpioinfo_request(gi, label);
}

static void gpioinfo_free(struct gpio_info *gi)
{
	if (!gi->requested)
		return;

	if (gi->chip->ops->free)
		gi->chip->ops->free(gi->chip, gpioinfo_chip_offset(gi));

	gi->requested = false;
	gi->active_low = false;
	free(gi->label);
	gi->label = NULL;
}

void gpio_free(unsigned gpio)
{
	struct gpio_info *gi = gpio_to_desc(gpio);

	if (!gi)
		return;

	gpioinfo_free(gi);
}

static void gpioinfo_set_value(struct gpio_info *gi, int value)
{
	if (gi->chip->ops->set)
		gi->chip->ops->set(gi->chip, gpioinfo_chip_offset(gi), value);
}

void gpio_set_value(unsigned gpio, int value)
{
	struct gpio_info *gi = gpio_to_desc(gpio);

	if (!gi)
		return;

	if (gpio_ensure_requested(gi, gpio))
		return;

	gpioinfo_set_value(gi, value);
}
EXPORT_SYMBOL(gpio_set_value);

void gpio_set_active(unsigned gpio, bool value)
{
	struct gpio_info *gi = gpio_to_desc(gpio);

	if (!gi)
		return;

	gpio_set_value(gpio, gpio_adjust_value(gi, value));
}
EXPORT_SYMBOL(gpio_set_active);

static int gpioinfo_get_value(struct gpio_info *gi)
{
	if (!gi->chip->ops->get)
		return -ENOSYS;

	return gi->chip->ops->get(gi->chip, gpioinfo_chip_offset(gi));
}

int gpio_get_value(unsigned gpio)
{
	struct gpio_info *gi = gpio_to_desc(gpio);
	int ret;

	if (!gi)
		return -ENODEV;

	ret = gpio_ensure_requested(gi, gpio);
	if (ret)
		return ret;

	return gpioinfo_get_value(gi);
}
EXPORT_SYMBOL(gpio_get_value);

int gpio_is_active(unsigned gpio)
{
	struct gpio_info *gi = gpio_to_desc(gpio);

	if (!gi)
		return -ENODEV;

	return gpio_adjust_value(gi, gpio_get_value(gpio));
}
EXPORT_SYMBOL(gpio_is_active);

static int gpioinfo_direction_output(struct gpio_info *gi, int value)
{
	if (!gi->chip->ops->direction_output)
		return -ENOSYS;

	return gi->chip->ops->direction_output(gi->chip,
					       gpioinfo_chip_offset(gi), value);
}

int gpio_direction_output(unsigned gpio, int value)
{
	struct gpio_info *gi = gpio_to_desc(gpio);
	int ret;

	if (!gi)
		return -ENODEV;

	ret = gpio_ensure_requested(gi, gpio);
	if (ret)
		return ret;

	return gpioinfo_direction_output(gi, value);
}
EXPORT_SYMBOL(gpio_direction_output);

static int gpioinfo_direction_active(struct gpio_info *gi, bool value)
{
	return gpioinfo_direction_output(gi, gpio_adjust_value(gi, value));
}

int gpio_direction_active(unsigned gpio, bool value)
{
	struct gpio_info *gi = gpio_to_desc(gpio);

	if (!gi)
		return -ENODEV;

	return gpioinfo_direction_active(gi, value);
}
EXPORT_SYMBOL(gpio_direction_active);

static int gpioinfo_direction_input(struct gpio_info *gi)
{
	if (!gi->chip->ops->direction_input)
		return -ENOSYS;

	return gi->chip->ops->direction_input(gi->chip,
					      gpioinfo_chip_offset(gi));
}

int gpio_direction_input(unsigned gpio)
{
	struct gpio_info *gi = gpio_to_desc(gpio);
	int ret;

	if (!gi)
		return -ENODEV;

	ret = gpio_ensure_requested(gi, gpio);
	if (ret)
		return ret;

	return gpioinfo_direction_input(gi);
}
EXPORT_SYMBOL(gpio_direction_input);

static int gpioinfo_request_one(struct gpio_info *gi, unsigned long flags,
				const char *label)
{
	int err;

	/*
	 * Not all of the flags below are mulit-bit, but, for the sake
	 * of consistency, the code is written as if all of them were.
	 */
	const bool active_low  = (flags & GPIOF_ACTIVE_LOW) == GPIOF_ACTIVE_LOW;
	const bool dir_in      = (flags & GPIOF_DIR_IN) == GPIOF_DIR_IN;
	const bool logical     = (flags & GPIOF_LOGICAL) == GPIOF_LOGICAL;
	const bool init_active = (flags & GPIOF_INIT_ACTIVE) == GPIOF_INIT_ACTIVE;
	const bool init_high   = (flags & GPIOF_INIT_HIGH) == GPIOF_INIT_HIGH;

	err = gpioinfo_request(gi, label);
	if (err)
		return err;

	gi->active_low = active_low;

	if (dir_in)
		err = gpioinfo_direction_input(gi);
	else if (logical)
		err = gpioinfo_direction_active(gi, init_active);
	else
		err = gpioinfo_direction_output(gi, init_high);

	if (err)
		gpioinfo_free(gi);

	return err;
}

/**
 * gpio_request_one - request a single GPIO with initial configuration
 * @gpio:	the GPIO number
 * @flags:	GPIO configuration as specified by GPIOF_*
 * @label:	a literal description string of this GPIO
 */
int gpio_request_one(unsigned gpio, unsigned long flags, const char *label)
{
	struct gpio_info *gi = gpio_to_desc(gpio);

	if (!gi)
		return -ENODEV;

	return gpioinfo_request_one(gi, flags, label);
}
EXPORT_SYMBOL_GPL(gpio_request_one);

/**
 * gpio_request_array - request multiple GPIOs in a single call
 * @array:	array of the 'struct gpio'
 * @num:	how many GPIOs in the array
 */
int gpio_request_array(const struct gpio *array, size_t num)
{
	int i, err;

	for (i = 0; i < num; i++, array++) {
		err = gpio_request_one(array->gpio, array->flags, array->label);
		if (err)
			goto err_free;
	}
	return 0;

err_free:
	while (i--)
		gpio_free((--array)->gpio);
	return err;
}
EXPORT_SYMBOL_GPL(gpio_request_array);

/**
 * gpio_free_array - release multiple GPIOs in a single call
 * @array:	array of the 'struct gpio'
 * @num:	how many GPIOs in the array
 */
void gpio_free_array(const struct gpio *array, size_t num)
{
	while (num--)
		gpio_free((array++)->gpio);
}
EXPORT_SYMBOL_GPL(gpio_free_array);

int gpio_array_to_id(const struct gpio *array, size_t num, u32 *val)
{
	u32 id = 0;
	int ret, i;

	if (num > 32)
		return -EOVERFLOW;

	ret = gpio_request_array(array, num);
	if (ret)
		return ret;

	/* Wait until logic level will be stable */
	udelay(5);
	for (i = 0; i < num; i++) {
		ret = gpio_is_active(array[i].gpio);
		if (ret < 0)
			goto free_array;
		if (ret)
			id |= 1UL << i;
	}

	*val = id;
	ret = 0;

free_array:
	gpio_free_array(array, num);
	return ret;
}
EXPORT_SYMBOL(gpio_array_to_id);

static int gpiochip_find_base(int start, int ngpio)
{
	int i;
	int spare = 0;
	int base = -ENOSPC;

	if (start < 0)
		start = 0;

	for (i = start; i < ARCH_NR_GPIOS; i++) {
		struct gpio_chip *chip = gpio_desc[i].chip;

		if (!chip) {
			spare++;
			if (spare == ngpio) {
				base = i + 1 - ngpio;
				break;
			}
		} else {
			spare = 0;
			i += chip->ngpio - 1;
		}
	}

	if (gpio_is_valid(base))
		debug("%s: found new base at %d\n", __func__, base);
	return base;
}

static int of_hog_gpio(struct device_node *np, struct gpio_chip *chip,
		       unsigned int idx)
{
	struct device_node *chip_np = chip->dev->device_node;
	unsigned long flags = 0;
	u32 gpio_cells, gpio_num, gpio_flags;
	int ret, gpio;
	const char *name = NULL;

	ret = of_property_read_u32(chip_np, "#gpio-cells", &gpio_cells);
	if (ret)
		return ret;

	/*
	 * Support for GPIOs that don't have #gpio-cells set to 2 is
	 * not implemented
	 */
	if (WARN_ON(gpio_cells != 2))
		return -ENOTSUPP;

	ret = of_property_read_u32_index(np, "gpios", idx * gpio_cells,
					 &gpio_num);
	if (ret)
		return ret;

	ret = of_property_read_u32_index(np, "gpios", idx * gpio_cells + 1,
					 &gpio_flags);
	if (ret)
		return ret;

	if (gpio_flags & OF_GPIO_ACTIVE_LOW)
		flags |= GPIOF_ACTIVE_LOW;

	gpio = gpio_get_num(chip->dev, gpio_num);
	if (gpio == -EPROBE_DEFER)
		return gpio;

	if (gpio < 0) {
		dev_err(chip->dev, "unable to get gpio %u\n", gpio_num);
		return gpio;
	}


	/*
	 * Note that, in order to be compatible with Linux, the code
	 * below interprets 'output-high' as to mean 'output-active'.
	 * That is, when processed for active-low GPIO, it will result
	 * in output being asserted logically 'active', but physically
	 * 'low'.
	 *
	 * Conversely it means that specifying 'output-low' for
	 * 'active-low' GPIO would result in 'high' level observed on
	 * the corresponding pin
	 *
	 */
	if (of_property_read_bool(np, "input"))
		flags |= GPIOF_DIR_IN;
	else if (of_property_read_bool(np, "output-low"))
		flags |= GPIOF_OUT_INIT_INACTIVE;
	else if (of_property_read_bool(np, "output-high"))
		flags |= GPIOF_OUT_INIT_ACTIVE;
	else
		return -EINVAL;

	/* The line-name is optional and if not present the node name is used */
	ret = of_property_read_string(np, "line-name", &name);
	if (ret < 0)
		name = np->name;

	return gpio_request_one(gpio, flags, name);
}

static int of_gpiochip_scan_hogs(struct gpio_chip *chip)
{
	struct device_node *np;
	int ret, i, count;

	if (!IS_ENABLED(CONFIG_OFDEVICE) || !chip->dev->device_node)
		return 0;

	count = of_property_count_strings(chip->dev->device_node, "gpio-line-names");

	if (count > 0) {
		const char **arr = xzalloc(count * sizeof(char *));

		of_property_read_string_array(chip->dev->device_node,
					      "gpio-line-names", arr, count);

		for (i = 0; i < chip->ngpio && i < count; i++)
			gpio_desc[chip->base + i].name = xstrdup(arr[i]);

		free(arr);
	}

	for_each_available_child_of_node(chip->dev->device_node, np) {
		if (!of_property_read_bool(np, "gpio-hog"))
			continue;

		for (ret = 0, i = 0;
		     !ret;
		     ret = of_hog_gpio(np, chip, i), i++)
			;
		/*
		 * We ignore -EOVERFLOW because it serves us as an
		 * indicator that there's no more GPIOs to handle.
		 */
		if (ret < 0 && ret != -EOVERFLOW)
			return ret;
	}

	return 0;
}

static const char *gpio_suffixes[] = {
	"gpios",
	"gpio",
};

/* Linux compatibility helper: Get a GPIO descriptor from device tree */
int __gpiod_get(struct device_d *dev, const char *_con_id, int idx, enum gpiod_flags flags)
{
	struct device_node *np = dev->device_node;
	enum of_gpio_flags of_flags;
	const char *label = dev_name(dev);
	char *buf = NULL, *con_id;
	int gpio;
	int ret, i;

	if (!IS_ENABLED(CONFIG_OFDEVICE) || !dev->device_node)
		return -ENODEV;

	for (i = 0; i < ARRAY_SIZE(gpio_suffixes); i++) {
		if (_con_id)
			con_id = basprintf("%s-%s", _con_id, gpio_suffixes[i]);
		else
			con_id = basprintf("%s", gpio_suffixes[i]);

		if (!con_id)
			return -ENOMEM;

		gpio = of_get_named_gpio_flags(np, con_id, idx, &of_flags);
		free(con_id);

		if (gpio_is_valid(gpio))
			break;
	}

	if (!gpio_is_valid(gpio))
		return gpio < 0 ? gpio : -EINVAL;

	if (of_flags & OF_GPIO_ACTIVE_LOW)
		flags |= GPIOF_ACTIVE_LOW;

	buf = NULL;

	if (_con_id) {
		label = buf = basprintf("%s-%s", dev_name(dev), _con_id);
		if (!label)
			return -ENOMEM;
	}

	ret = gpio_request_one(gpio, flags, label);
	free(buf);

	return ret ?: gpio;
}

int gpiochip_add(struct gpio_chip *chip)
{
	int base, i;

	base = gpiochip_find_base(chip->base, chip->ngpio);
	if (base < 0)
		return base;

	if (chip->base >= 0 && chip->base != base)
		return -EBUSY;

	chip->base = base;

	list_add_tail(&chip->list, &chip_list);

	for (i = chip->base; i < chip->base + chip->ngpio; i++)
		gpio_desc[i].chip = chip;

	return of_gpiochip_scan_hogs(chip);
}

void gpiochip_remove(struct gpio_chip *chip)
{
	list_del(&chip->list);
}

int gpio_get_num(struct device_d *dev, int gpio)
{
	struct gpio_chip *chip;

	if (!dev)
		return -ENODEV;

	list_for_each_entry(chip, &chip_list, list) {
		if (chip->dev == dev)
			return chip->base + gpio;
	}

	return -EPROBE_DEFER;
}

struct gpio_chip *gpio_get_chip(int gpio)
{
	struct gpio_info *gi = gpio_to_desc(gpio);

	return gi ? gi->chip : NULL;
}

#ifdef CONFIG_CMD_GPIO
static int do_gpiolib(int argc, char *argv[])
{
	int i;

	for (i = 0; i < ARCH_NR_GPIOS; i++) {
		struct gpio_info *gi = &gpio_desc[i];
		int val = -1, dir = -1;

		if (!gi->chip)
			continue;

		/* print chip information and header on first gpio */
		if (gi->chip->base == i) {
			printf("\nGPIOs %u-%u, chip %s:\n",
				gi->chip->base,
				gi->chip->base + gi->chip->ngpio - 1,
				gi->chip->dev->name);
			printf("             %-3s %-3s %-9s %-20s %-20s\n", "dir", "val", "requested", "name", "label");
		}

		if (gi->chip->ops->get_direction)
			dir = gi->chip->ops->get_direction(gi->chip,
						i - gi->chip->base);
		if (gi->chip->ops->get)
			val = gi->chip->ops->get(gi->chip,
						i - gi->chip->base);

		printf("  GPIO %4d: %-3s %-3s %-9s %-20s %-20s\n", i,
			(dir < 0) ? "unk" : ((dir == GPIOF_DIR_IN) ? "in" : "out"),
			(val < 0) ? "unk" : ((val == 0) ? "lo" : "hi"),
		        gi->requested ? (gi->active_low ? "active low" : "true") : "false",
			gi->name ? gi->name : "",
			gi->label ? gi->label : "");
	}

	return 0;
}

BAREBOX_CMD_START(gpioinfo)
	.cmd		= do_gpiolib,
	BAREBOX_CMD_DESC("list registered GPIOs")
	BAREBOX_CMD_GROUP(CMD_GRP_INFO)
	BAREBOX_CMD_COMPLETE(empty_complete)
BAREBOX_CMD_END
#endif

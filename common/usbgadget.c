// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2017 Oleksij Rempel <o.rempel@pengutronix.de>, Pengutronix
 */
#define pr_fmt(fmt) "usbgadget: " fmt

#include <common.h>
#include <command.h>
#include <errno.h>
#include <environment.h>
#include <malloc.h>
#include <getopt.h>
#include <fs.h>
#include <xfuncs.h>
#include <usb/usbserial.h>
#include <usb/dfu.h>
#include <usb/gadget-multi.h>
#include <globalvar.h>
#include <magicvar.h>
#include <system-partitions.h>

static int autostart;
static int acm;
static char *dfu_function;

static inline struct file_list *get_dfu_function(void)
{
	if (dfu_function && *dfu_function)
		return file_list_parse_null(dfu_function);
	if (!system_partitions_empty())
		return system_partitions_get();
	return NULL;
}

int usbgadget_register(const struct usbgadget_funcs *funcs)
{
	int ret;
	int flags = funcs->flags;
	struct device_d *dev;
	struct f_multi_opts *opts;

	opts = xzalloc(sizeof(*opts));
	opts->release = usb_multi_opts_release;

	if (flags & USBGADGET_DFU) {
		opts->dfu_opts.files = file_list_parse_null(funcs->dfu_opts);
		if (IS_ENABLED(CONFIG_USB_GADGET_DFU) && file_list_empty(opts->dfu_opts.files)) {
			file_list_free(opts->dfu_opts.files);
			opts->dfu_opts.files = get_dfu_function();
		}
	}

	if (flags & USBGADGET_MASS_STORAGE) {
		opts->ums_opts.files = file_list_parse_null(funcs->ums_opts);
		if (IS_ENABLED(CONFIG_USB_GADGET_MASS_STORAGE) && file_list_empty(opts->ums_opts.files)) {
			file_list_free(opts->ums_opts.files);
			opts->ums_opts.files = system_partitions_get();
		}
	}

	if (flags & USBGADGET_FASTBOOT) {
		opts->fastboot_opts.files = file_list_parse_null(funcs->fastboot_opts);
		if (IS_ENABLED(CONFIG_FASTBOOT_BASE) && file_list_empty(opts->fastboot_opts.files)) {
			file_list_free(opts->fastboot_opts.files);
			opts->fastboot_opts.files = get_fastboot_partitions();
		}

		opts->fastboot_opts.export_bbu = flags & USBGADGET_EXPORT_BBU;
	}

	opts->create_acm = flags & USBGADGET_ACM;

	if (usb_multi_count_functions(opts) == 0) {
		pr_warn("No functions to register\n");
		ret = COMMAND_ERROR_USAGE;
		goto err;
	}

	/*
	 * Creating a gadget with both DFU and Fastboot may not work.
	 * fastboot 1:8.1.0+r23-5 can deal with it, but dfu-util 0.9
	 * seems to assume that the device only has a single configuration
	 * That's not our fault though. Emit a warning and continue
	 */
	if (!file_list_empty(opts->fastboot_opts.files) && !file_list_empty(opts->dfu_opts.files))
		pr_warn("Both DFU and Fastboot enabled. dfu-util may not like this!\n");

	dev = get_device_by_name("otg");
	if (dev)
		dev_set_param(dev, "mode", "peripheral");

	ret = usb_multi_register(opts);
	if (ret)
		goto err;

	return 0;
err:
	usb_multi_opts_release(opts);

	return ret;
}

static int usbgadget_autostart_set(struct param_d *param, void *ctx)
{
	struct usbgadget_funcs funcs = {};
	static bool started;
	int err;

	if (!autostart || started)
		return 0;

	if (get_fastboot_bbu())
		funcs.flags |= USBGADGET_EXPORT_BBU;
	if (acm)
		funcs.flags |= USBGADGET_ACM;

	funcs.flags |= USBGADGET_DFU | USBGADGET_FASTBOOT | USBGADGET_MASS_STORAGE;

	err = usbgadget_register(&funcs);
	if (!err)
		started = true;

	return err;
}

static int usbgadget_globalvars_init(void)
{
	globalvar_add_simple_bool("usbgadget.acm", &acm);
	globalvar_add_simple_string("usbgadget.dfu_function", &dfu_function);

	return 0;
}
device_initcall(usbgadget_globalvars_init);

static int usbgadget_autostart_init(void)
{
	if (IS_ENABLED(CONFIG_USB_GADGET_AUTOSTART))
		globalvar_add_bool("usbgadget.autostart", usbgadget_autostart_set, &autostart, NULL);
	return 0;
}
postenvironment_initcall(usbgadget_autostart_init);

BAREBOX_MAGICVAR(global.usbgadget.autostart,
		 "usbgadget: Automatically start usbgadget on boot");
BAREBOX_MAGICVAR(global.usbgadget.acm,
		 "usbgadget: Create CDC ACM function");
BAREBOX_MAGICVAR(global.usbgadget.dfu_function,
		 "usbgadget: Create DFU function");

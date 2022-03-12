// SPDX-License-Identifier: GPL-2.0-only

#define pr_fmt(fmt) "v7m-bootm: " fmt

#include <bootm.h>
#include <common.h>
#include <init.h>
#include <memory.h>
#include <asm/barebox-arm.h>

#define CHAINLOADED_MAGIC	0x10AD10AD

static void __noreturn __naked armv7m_xload(u32 sp,
					    void __noreturn (*bb)(u32 r0, u32 r1, u32 r2))
{
	arm_setup_stack(sp);
	/*
	 * Let new barebox know it was chainloaded. Perhaps this turns
	 * out useful in future
	 */
	bb(CHAINLOADED_MAGIC, CHAINLOADED_MAGIC, CHAINLOADED_MAGIC);
}

static int do_bootm_v7m(struct image_data *data)
{
	void *reset_vector;
	resource_size_t start, end;
	u32 *ivt, stack_pointer, text_base;
	int ret;

	ret = memory_bank_first_find_space(&start, &end);
	if (ret)
		return ret;

	ret = bootm_load_os(data, start);
	if (ret)
		return ret;

	ivt = (void *)start;

	stack_pointer = ivt[0];
	text_base = ivt[ARM_HEAD_TEXT_BASE / sizeof(u32)];
	reset_vector = (void *)start + ivt[1] - text_base;

	if (data->verbose)
		printf("Loaded barebox image to %p with stack at %x\n",
		       reset_vector, stack_pointer);

	shutdown_barebox();

	armv7m_xload(stack_pointer, reset_vector);

	return -EIO;
}

static struct image_handler v7m_barebox_image_handler = {
	.name = "ARMv7-M barebox",
	.bootm = do_bootm_v7m,
	.filetype = filetype_armv7m_barebox,
};

static int v7m_barebox_image_handler_register(void)
{
	return register_image_handler(&v7m_barebox_image_handler);
}
late_initcall(v7m_barebox_image_handler_register);

/* SPDX-License-Identifier: GPL-2.0-only */

#include <asm/barebox-arm.h>
#include <linux/string.h>
#include <asm-generic/sections.h>

void setup_c(void)
{
	memset(__bss_start, 0, __bss_stop - __bss_start);
}

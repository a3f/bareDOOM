// SPDX-License-Identifier: GPL-2.0-only

#include <asm/sections.h>
#include <linux/types.h>

char __rel_dyn_start[0] __attribute__((section(".__rel_dyn_start")));
char __rel_dyn_end[0] __attribute__((section(".__rel_dyn_end")));
char __dynsym_start[0] __attribute__((section(".__dynsym_start")));
char __dynsym_end[0] __attribute__((section(".__dynsym_end")));
char _text[0] __attribute__((section("._text")));
char __bss_start[0] __attribute__((section(".__bss_start")));
char __bss_stop[0] __attribute__((section(".__bss_stop")));
char __got_start[0] __attribute__((section(".__got_start")));
char __got_stop[0] __attribute__((section(".__got_stop")));
char __rwdata_start[0] __attribute__((section(".__rwdata_start")));
char __rwdata_stop[0] __attribute__((section(".__rwdata_stop")));
char __image_start[0] __attribute__((section(".__image_start")));
char __image_end[0] __attribute__((section(".__image_end")));

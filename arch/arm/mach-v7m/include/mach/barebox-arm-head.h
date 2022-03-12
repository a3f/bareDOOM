/* SPDX-License-Identifier: GPL-2.0-only */

#ifndef __ASM_ARM_MACH_V7M_HEAD_H
#define __ASM_ARM_MACH_V7M_HEAD_H

static __always_inline void __barebox_arm_head(void)
{
	__asm__ __volatile__ (
		"ivt:\n"
#define __0REL(v) v " - (ivt - 4)"
#define __IV(v)	".word " __0REL(v) " + 1\n"
		__IV("reset")
		__IV("nmi")
		__IV("hard_fault")
		__IV("mm_fault")
		__IV("bus_fault")
		__IV("usage_fault")
#undef IV
		".ascii \"V7-M\"\n"
		".asciz \"barebox\"\n"
		".word 0\n"
		".word 0\n"
		".rept 8\n"
		".word 0x55555555\n"
		".endr\n"
#define __LOOP(e)  e ": b " e "\n"
		__LOOP("nmi")
		__LOOP("hard_fault")
		__LOOP("mm_fault")
		__LOOP("bus_fault")
		__LOOP("usage_fault")
#undef __LOOP
#define __REF(s)	s "_0rel: .word " __0REL(s)     "\n"
		__REF("__rwdata_start")
		__REF("__rwdata_stop")
		__REF("__got_start")
		__REF("__got_stop")
#undef __REF
		"reset:\n"
#ifdef CONFIG_PBL_XIP
		"mov r6, r0\n"	/* bank registers */
		"mov r7, r1\n"
		"mov r8, r2\n"
		"bl get_runtime_offset\n"
		"ldr r4, __rwdata_start_0rel\n"
		"ldr r5, __rwdata_stop_0rel\n"
		"mov r1, r4\n"
		"mov r2, r5\n"
		"ldr r9, __got_start_0rel\n"
		"ldr r10, __got_stop_0rel\n"
		"sub r10, r9\n"	/* calculate GOT length */
		"sub r9, r1\n"	/* get GOT relative to mutable data */
		"sub r2, r1\n"	/* calculate mutable data length */
		"add r1, r0\n"	/* calculate mutable data runtime offset */
		"add r2, 7\n"
		"and r2, ~7\n"	/* ensure 8-byte stack alignment */
		"sub sp, r2\n"	/* place mutable data above stack */
		"mov r0, sp\n"
		"bl __memcpy\n"	/* relocate rwdata (.bss + .data) */
		"add r9, r0\n"	/* GOT now in SRAM */
		"sub r0, r4\n"	/* addend for GOT fixups */
		"add r10, r9\n"	/* end is GOT end in SRAM */
		"b 2f\n"
		"1:\n"
		"sub r10, 4\n"	/* iterate backwards over GOT */
		"ldr r1, [r10]\n"
		"cmp r1, r4\n"
		"blt 2f\n"	/* bail if before rwdata */
		"cmp r1, r5\n"
		"bge 2f\n"	/* bail if after rwdata */
		"add r1, r0\n"
		"str r1, [r10]\n"
		"2:\n"
		"cmp r10, r9\n"
		"bgt 1b\n"
		"mov r0, r6\n"
		"mov r1, r7\n"
		"mov r2, r8\n"
#endif
#ifdef CONFIG_PBL_BREAK
		"bkpt #17\n"
		"nop\n"
#else
		"nop\n"
		"nop\n"
#endif
#undef __0REL
	);
}

#define ENTRY_FUNCTION_WITHSTACK(name, stack_top, arg0, arg1, arg2)	\
	void name (uint32_t r0, uint32_t r1, uint32_t r2);		\
									\
	static void __##name(uint32_t, uint32_t, uint32_t);		\
									\
	void __naked __section(.text_head_entry_##name)	name		\
				(uint32_t r0, uint32_t r1, uint32_t r2)	\
		{							\
			__asm__ __volatile__ (".word " #stack_top);	\
			__barebox_arm_head();				\
			__##name(r0, r1, r2);				\
		}							\
		static void noinline __##name				\
			(uint32_t arg0, uint32_t arg1, uint32_t arg2)

#endif

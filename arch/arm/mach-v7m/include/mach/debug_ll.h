#ifndef __MACH_V7M_DEBUG_LL_H
#define __MACH_V7M_DEBUG_LL_H

#ifdef CONFIG_DEBUG_SEMIHOSTING_WRITEC

static inline void PUTC_LL(char c)
{
	/* We may not be relocated yet here, so we push
	 * to stack and take address of that
	 */
	__asm__ volatile (
		 "push {%0}\n"
		 "mov r1, sp\n"
		 "mov r0, #0x03\n"
		 "bkpt #0xAB\n"
		 "pop {%0}\n"
		 : /* No outputs */
		 : "r" (c)
		 : "r0", "r1", "r2", "r3", "ip", "lr", "memory", "cc"
	);
}

#endif

#endif /* __MACH_V7M_DEBUG_LL_H */

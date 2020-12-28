#ifndef DOOMSTDLIB_H_
#define DOOMSTDLIB_H_

#ifdef __BAREBOX__
#include <linux/kernel.h>
#include <asm/setjmp.h>
#define strtol(...) simple_strtol(__VA_ARGS__)
extern jmp_buf exit_jmpbuf;
#define EXIT_SUCCESS 0x100
static inline void exit(int code)
{
	longjmp(exit_jmpbuf, code ?: EXIT_SUCCESS);
}
#else
#include_next <stdlib.h>
#endif

#define atoi(str) strtol((str), NULL, 10)

#endif

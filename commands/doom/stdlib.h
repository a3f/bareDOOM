#ifndef DOOMSTDLIB_H_
#define DOOMSTDLIB_H_

#include <linux/kernel.h>
#include <asm/setjmp.h>
#define strtol(...) simple_strtol(__VA_ARGS__)
extern jmp_buf exit_jmpbuf;
#define EXIT_SUCCESS 0x100
static inline void exit(int code)
{
	longjmp(exit_jmpbuf, code ?: EXIT_SUCCESS);
}

#define atoi(str) strtol((str), NULL, 10)

#endif

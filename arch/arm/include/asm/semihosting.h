/* SPDX-License-Identifier: GPL-2.0-only */

#ifndef __ASM_ARM_SEMIHOSTING_H
#define __ASM_ARM_SEMIHOSTING_H

int semihosting_open(const char *fname, int flags);
int semihosting_close(int fd);
int semihosting_writec(char c);
int semihosting_write0(const char *str);
ssize_t semihosting_write(int fd, const void *buf, size_t count);
ssize_t semihosting_read(int fd, void *buf, size_t count);
int semihosting_readc(void);
int semihosting_isatty(int fd);
int semihosting_seek(int fd, loff_t pos);
int semihosting_flen(int fd);
int semihosting_remove(const char *fname);
int semihosting_rename(const char *fname1, const char *fname2);
int semihosting_errno(void);
int semihosting_system(const char *command);

enum {
	SEMIHOSTING_SYS_OPEN	= 0x01,
	SEMIHOSTING_SYS_CLOSE	= 0x02,
	SEMIHOSTING_SYS_WRITEC	= 0x03,
	SEMIHOSTING_SYS_WRITE0	= 0x04,
	SEMIHOSTING_SYS_WRITE	= 0x05,
	SEMIHOSTING_SYS_READ	= 0x06,
	SEMIHOSTING_SYS_READC	= 0x07,
	/* SYS_ISERROR is not implemented  */
	SEMIHOSTING_SYS_ISATTY	= 0x09,
	SEMIHOSTING_SYS_SEEK	= 0x0a,
	SEMIHOSTING_SYS_FLEN	= 0x0c,
	SEMIHOSTING_SYS_REMOVE	= 0x0e,
	SEMIHOSTING_SYS_RENAME	= 0x0f,
	SEMIHOSTING_SYS_TIME	= 0x11,
	SEMIHOSTING_SYS_ERRNO	= 0x13,
	/* SYS_GET_CMDLINE is not implemented */
	/* SYS_HEAPINFO is not implemented */
	/* angel_SWIreason_ReportException is not implemented */
	SEMIHOSTING_SYS_SYSTEM	= 0x12,
};

uint32_t semihosting_trap(uint32_t sysnum, void *addr);

#endif

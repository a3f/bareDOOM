// SPDX-License-Identifier: GPL-2.0

#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <libfile.h>

#include "doomtype.h"
#include "stdio.h"

static long errno_wrap(long ret)
{
	if (ret >= 0)
		return ret;

	errno = ret;
	return -1;
}

FILE *fopen(const char *filename, const char *mode)
{
	int fd, flags = 0;

	if (strchr(filename, 'r'))
		flags |= O_RDONLY;
	else if (strchr(filename, 'w'))
		flags |= O_WRONLY | O_TRUNC;

	fd = open(filename, flags);
	if (errno_wrap(fd) < 0)
		return NULL;

	return fd2fp(fd);
}

int fclose(FILE *fp)
{
	close(fp2fd(fp));
	return 0;
}

size_t fwrite(const void *ptr, size_t size, size_t nmemb,
	     FILE *fp)
{
	int ret = errno_wrap(write(fp2fd(fp), ptr, size * nmemb));
	return ret < 0 ? ret : ret / size;
}

size_t fread(void *ptr, size_t size, size_t nmemb, FILE *fp)
{
	int ret = errno_wrap(read(fp2fd(fp), ptr, size * nmemb));
	return ret < 0 ? ret : ret / size;
}

int fseek(FILE *fp, long offset, int whence)
{
	return errno_wrap(lseek(fp2fd(fp), offset, whence)) < 0 ? -1 : 0;
}

long ftell(FILE *fp)
{
	return errno_wrap(lseek(fp2fd(fp), 0, SEEK_CUR));
}

int fflush(FILE *fp)
{
	return 0;
}

int remove(const char *pathname)
{
	return errno_wrap(unlink(pathname));
}

int rename(const char *oldpath, const char *newpath)
{
	return errno_wrap(copy_file(oldpath, newpath, 1));
}

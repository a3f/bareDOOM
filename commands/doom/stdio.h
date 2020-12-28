#ifndef DOOMSTDIO_H_
#define DOOMSTDIO_H_

#include "doomtype.h"

#ifdef __BAREBOX__
typedef const void FILE;

#define fd2fp(fd) ((FILE *)(uintptr_t)fd)
#define fp2fd(fp) ((int)(uintptr_t)fp)

#define stdout fd2fp(0)
#define stderr fd2fp(0)

FILE *fopen(const char *filename, const char *mode);
int fclose(FILE *fp);
size_t fwrite(const void *ptr, size_t size, size_t nmemb,
	     FILE *fp);

size_t fread(void *ptr, size_t size, size_t nmemb, FILE *fp);

int fflush(FILE *fp);
long ftell(FILE *fp);
int remove(const char *pathname);
int rename(const char *oldpath, const char *newpath);
int fseek(FILE *fp, long offset, int whence);
int mkdir (const char *pathname, mode_t mode);

enum { SEEK_SET = 0, SEEK_CUR = 1, SEEK_END = 2 };

#define fprintf(fp, ...) dprintf(fp2fd(fp), __VA_ARGS__)
#define vfprintf(fp, ...) vprintf(__VA_ARGS__)

#endif

#include_next <../../include/stdio.h>

#endif

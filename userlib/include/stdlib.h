#ifndef __CPL1_LIBC_STDLIB_H_INCLUDED__
#define __CPL1_LIBC_STDLIB_H_INCLUDED__

#include <stddef.h>

void *malloc(size_t size);
void free(void *ptr);
void *calloc(size_t nmemb, size_t size);
void *realloc(void *ptr, size_t size);
void *reallocarray(void *ptr, size_t nmemb, size_t size);

#define EXIT_SUCCESS 0
#define EXIT_FAILURE -1
void exit(int exitCode);

#endif

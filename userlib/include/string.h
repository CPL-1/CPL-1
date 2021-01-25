#ifndef __CPL1_LIBC_STRING_H_INCLUDED__
#define __CPL1_LIBC_STRING_H_INCLUDED__

#include <stddef.h>

size_t strlen(const char *s);
void *memcpy(void *dest, const void *src, size_t n);
void *memset(void *s, int c, size_t n);
char *strcat(char *dest, const char *src);
int strcmp(const char *s1, const char *s2);

#endif
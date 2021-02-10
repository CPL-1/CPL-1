#ifndef __CPL1_LIBC_STRING_H_INCLUDED__
#define __CPL1_LIBC_STRING_H_INCLUDED__

#include <stddef.h>

size_t strlen(const char *s);
void *memcpy(void *dest, const void *src, size_t n);
void *memset(void *s, int c, size_t n);
char *strcat(char *dest, const char *src);
int memcmp(const void *s1, const void *s2, size_t n);
int strcmp(const char *s1, const char *s2);
int strncmp(const char *s1, const char *s2, size_t n);
char *strcpy(char *dest, const char *src);
char *stpcpy(char *dest, const char *src);
char *strdup(const char *s);
char *strchr(const char *s, int c);
char *strrchr(const char *s, int c);
size_t strnlen(const char *s, size_t maxlen);
size_t strspn(const char *s, const char *accept);
size_t strcspn(const char *s, const char *reject);
char *strsep(char **stringp, const char *delim);
int strcasecmp(const char *s1, const char *s2);
int strncasecmp(const char *s1, const char *s2, size_t n);

#endif

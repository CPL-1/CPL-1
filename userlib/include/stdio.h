#ifndef __CPL1_LIBC_STDIO_H_INCLUDED__
#define __CPL1_LIBC_STDIO_H_INCLUDED__

#include <stdarg.h>
#include <stddef.h>

int printf(const char *fmt, ...);
int snprintf(const char *fmt, char *buf, int size, ...);
int va_printf(const char *fmt, va_list args);
int va_sprintf(const char *fmt, char *buf, int size, va_list args);
int puts(const char *str);

#define FILE struct __FILE

FILE *fopen(const char *name, const char *attr);
size_t fread(void *buf, size_t size, size_t nmemb, FILE *stream);
size_t fwrite(const void *buf, size_t size, size_t nmemb, FILE *stream);
int fclose(FILE *stream);

#endif
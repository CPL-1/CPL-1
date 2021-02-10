#ifndef __CPL1_LIBC_STDIO_H_INCLUDED__
#define __CPL1_LIBC_STDIO_H_INCLUDED__

#include <stdarg.h>
#include <stddef.h>

#define EOF -1

int printf(const char *fmt, ...);
int snprintf(char *buf, int size, const char *fmt, ...);
int va_printf(const char *fmt, va_list args);
int va_snprintf(char *buf, int size, const char *fmt, va_list args);
int puts(const char *str);

int getchar();
int putchar(int c);

#endif

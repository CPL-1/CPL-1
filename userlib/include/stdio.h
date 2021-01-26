#ifndef __CPL1_LIBC_STDIO_H_INCLUDED__
#define __CPL1_LIBC_STDIO_H_INCLUDED__

#include <stdarg.h>
#include <stddef.h>

int printf(const char *fmt, ...);
int snprintf(const char *fmt, char *buf, int size, ...);
int va_printf(const char *fmt, va_list args);
int va_sprintf(const char *fmt, char *buf, int size, va_list args);
int puts(const char *str);

#endif
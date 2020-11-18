#ifndef __PRINTF_H_INCLUDED__
#define __PRINTF_H_INCLUDED__

#include <stdarg.h>
#include <stddef.h>

size_t printf(const char *fmt, ...);
size_t sprintf(const char *fmt, char *buf, size_t size, ...);
size_t va_printf(const char *fmt, va_list args);
size_t va_sprintf(const char *fmt, char *buf, size_t size, va_list args);

#endif

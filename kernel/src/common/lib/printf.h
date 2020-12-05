#ifndef __printf_H_INCLUDED__
#define __printf_H_INCLUDED__

#include <common/misc/types.h>
#include <stdarg.h>

size_t printf(const char *fmt, ...);
size_t sprintf(const char *fmt, char *buf, size_t size, ...);
size_t va_printf(const char *fmt, va_list args);
size_t va_sprintf(const char *fmt, char *buf, size_t size, va_list args);

#endif

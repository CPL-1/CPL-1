#ifndef __printf_H_INCLUDED__
#define __printf_H_INCLUDED__

#include <common/misc/types.h>
#include <stdarg.h>

USIZE printf(const char *fmt, ...);
USIZE sprintf(const char *fmt, char *buf, USIZE size, ...);
USIZE va_printf(const char *fmt, va_list args);
USIZE va_sprintf(const char *fmt, char *buf, USIZE size, va_list args);

#endif

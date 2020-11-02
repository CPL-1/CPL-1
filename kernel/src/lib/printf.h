#ifndef __PRINTF_H_INCLUDED__
#define __PRINTF_H_INCLUDED__

#include <stdarg.h>

void printf(const char *fmt, ...);
void va_printf(const char *fmt, va_list args);

#endif
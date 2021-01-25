#ifndef __CPL1_LIBC_STDARG_H_INCLUDED__
#define __CPL1_LIBC_STDARG_H_INCLUDED__

typedef __builtin_va_list va_list;

#define va_start(ap, param) __builtin_va_start(ap, param)
#define va_end(ap) __builtin_va_end(ap)
#define va_arg(ap, type) __builtin_va_arg(ap, type)

#endif
#ifndef __CPL1_LIBC_STDDEF_H_INCLUDED__
#define __CPL1_LIBC_STDDEF_H_INCLUDED__

// TODO: use proper definitions

#define NULL ((void *)0)
#define offsetof(type, member) __builtin_offsetof(type, member)
typedef long ptrdiff_t;
typedef unsigned long size_t;

#ifndef __cplusplus
typedef unsigned int wchar_t;
#endif

#endif

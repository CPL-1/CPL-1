#ifndef __ATTRIBUTES_H_INCLUDED__
#define __ATTRIBUTES_H_INCLUDED__

#define asm __asm__
#define inline __inline__
#define typeof __typeof__
#define auto __auto_type
#define packed __attribute__((packed))
#define unused __attribute__((unused))
#define packed_align(align) __attribute__((aligned(align))) packed
#define big_endian __attribute__((scalar_storage_order("big-endian")))
#define little_endian __attribute__((scalar_storage_order("little-endian")))

#endif
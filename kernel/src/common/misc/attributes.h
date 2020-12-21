#ifndef __ATTRIBUTES_H_INCLUDED__
#define __ATTRIBUTES_H_INCLUDED__

#define ASM __asm__
#define VOLATILE __volatile__
#define INLINE __inline__
#define TYPEOF __typeof__
#define AUTO __auto_type
#define PACKED __attribute__((packed))
#define MAYBE_UNUSED __attribute__((unused))
#define PACKED_ALIGN(align) __attribute__((aligned(align))) PACKED
#define NOALIGN __attribute__((aligned(1)))
#define BIG_ENDIAN __attribute__((scalar_storage_order("big-endian")))
#define LITTLE_ENDIAN __attribute__((scalar_storage_order("little-endian")))

#endif

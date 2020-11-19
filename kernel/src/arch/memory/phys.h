#ifndef __ARCH_PHYS_H_INCLUDED__
#define __ARCH_PHYS_H_INCLUDED__

#include <utils/utils.h>

uintptr_t arch_phys_krnl_alloc_area(uintptr_t size);
void arch_phys_krnl_free_area(uintptr_t area, size_t size);

uintptr_t arch_phys_user_alloc_frame();
void arch_phys_user_free_frame(uintptr_t frame);

#endif
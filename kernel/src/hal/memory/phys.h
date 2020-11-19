#ifndef __HAL_PHYS_H_INCLUDED__
#define __HAL_PHYS_H_INCLUDED__

#include <utils/utils.h>

uintptr_t hal_phys_krnl_alloc_area(uintptr_t size);
void hal_phys_krnl_free_area(uintptr_t area, size_t size);

uintptr_t hal_phys_user_alloc_frame();
void hal_phys_user_free_frame(uintptr_t frame);

#endif
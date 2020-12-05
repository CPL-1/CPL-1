#ifndef __HAL_PHYS_H_INCLUDED__
#define __HAL_PHYS_H_INCLUDED__

#include <common/misc/utils.h>

uintptr_t HAL_PhysicalMM_KernelAllocArea(uintptr_t size);
void HAL_PhysicalMM_KernelFreeArea(uintptr_t area, size_t size);

uintptr_t HAL_PhysicalMM_UserAllocFrame();
void HAL_PhysicalMM_UserFreeFrame(uintptr_t frame);

#endif
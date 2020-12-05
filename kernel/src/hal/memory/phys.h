#ifndef __HAL_PHYS_H_INCLUDED__
#define __HAL_PHYS_H_INCLUDED__

#include <common/misc/utils.h>

UINTN HAL_PhysicalMM_KernelAllocArea(UINTN size);
void HAL_PhysicalMM_KernelFreeArea(UINTN area, USIZE size);

UINTN HAL_PhysicalMM_UserAllocFrame();
void HAL_PhysicalMM_UserFreeFrame(UINTN frame);

#endif
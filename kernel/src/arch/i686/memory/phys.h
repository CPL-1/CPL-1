#ifndef __I686_MEMORY_PHYSICAL_H_INCLUDED__
#define __I686_MEMORY_PHYSICAL_H_INCLUDED__

#include <common/misc/utils.h>

void i686_PhysicalMM_Initialize();

UINT32 i686_PhysicalMM_KernelAllocFrame();
void HAL_PhysicalMM_KernelFreeFrame(UINT32 frame);
UINT32 i686_PhysicalMM_GetMemorySize();

#endif

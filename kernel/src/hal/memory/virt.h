#ifndef __HAL_VIRT_H_INCLUDED__
#define __HAL_VIRT_H_INCLUDED__

#include <common/misc/utils.h>

enum
{
	HAL_VIRT_FLAGS_WRITABLE = 1,
	HAL_VIRT_FLAGS_EXECUTABLE = 2,
	HAL_VIRT_FLAGS_DISABLE_CACHE = 4,
	HAL_VIRT_FLAGS_USER_ACCESSIBLE = 8,
};

extern uintptr_t HAL_VirtualMM_KernelMappingBase;
extern uintptr_t HAL_VirtualMM_UserAreaStart;
extern uintptr_t HAL_VirtualMM_UserAreaEnd;
extern size_t HAL_VirtualMM_PageSize;

uintptr_t HAL_VirtualMM_MakeNewAddressSpace();
void HAL_VirtualMM_FreeAddressSpace(uintptr_t root);
void HAL_VirtualMM_SwitchToAddressSpace(uintptr_t root);
uintptr_t HAL_VirtualMM_GetCurrentAddressSpace();
bool HAL_VirtualMM_MapPageAt(uintptr_t root, uintptr_t vaddr, uintptr_t paddr, int flags);
uintptr_t HAL_VirtualMM_UnmapPageAt(uintptr_t root, uintptr_t vaddr);
void HAL_VirtualMM_ChangePagePermissions(uintptr_t root, uintptr_t vaddr, int flags);
uintptr_t HAL_VirtualMM_GetIOMapping(uintptr_t paddr, size_t size, bool cacheDisabled);
void HAL_VirtualMM_Flush();

#endif
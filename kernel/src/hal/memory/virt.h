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

extern UINTN HAL_VirtualMM_KernelMappingBase;
extern UINTN HAL_VirtualMM_UserAreaStart;
extern UINTN HAL_VirtualMM_UserAreaEnd;
extern USIZE HAL_VirtualMM_PageSize;

UINTN HAL_VirtualMM_MakeNewAddressSpace();
void HAL_VirtualMM_FreeAddressSpace(UINTN root);
void HAL_VirtualMM_SwitchToAddressSpace(UINTN root);
UINTN HAL_VirtualMM_GetCurrentAddressSpace();

bool HAL_VirtualMM_MapPageAt(UINTN root, UINTN vaddr, UINTN paddr, int flags);

UINTN HAL_VirtualMM_UnmapPageAt(UINTN root, UINTN vaddr);

void HAL_VirtualMM_ChangePagePermissions(UINTN root, UINTN vaddr, int flags);

UINTN HAL_VirtualMM_GetIOMapping(UINTN paddr, USIZE size, bool cacheDisabled);

void HAL_VirtualMM_Flush();

#endif
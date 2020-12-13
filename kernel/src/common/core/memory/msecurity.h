#ifndef __MSECURITY_H_INCLUDED__
#define __MSECURITY_H_INCLUDED__

#include <common/misc/utils.h>
#include <hal/memory/virt.h>

#define MSECURITY_URW (HAL_VIRT_FLAGS_USER_ACCESSIBLE | HAL_VIRT_FLAGS_WRITABLE | HAL_VIRT_FLAGS_READABLE)
#define MSECURITY_UW (HAL_VIRT_FLAGS_USER_ACCESSIBLE | HAL_VIRT_FLAGS_WRITABLE)
#define MSECURITY_UR (HAL_VIRT_FLAGS_USER_ACCESSIBLE | HAL_VIRT_FLAGS_READABLE)

bool MemorySecurity_VerifyMemoryRangePermissions(uintptr_t start, uintptr_t end, int flags);
int MemorySecurity_VerifyCString(uintptr_t start, int maxLength, int flags);
int MemorySecurity_VerifyNullTerminatedPointerList(uintptr_t start, int maxPointerCount, int flags);

#endif
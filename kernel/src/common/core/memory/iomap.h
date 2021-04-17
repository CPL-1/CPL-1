#ifndef __IOMAP_H_INCLUDED__
#define __IOMAP_H_INCLUDED__

#include <common/misc/utils.h>

void IOMap_Initialize();
uintptr_t IOMap_AllocateIOMapping(uintptr_t paddr, size_t size, bool cacheDisabled);
void IOMap_FreeIOMapping(uintptr_t vaddr, size_t size);

#endif

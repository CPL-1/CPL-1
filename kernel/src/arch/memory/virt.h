#ifndef __ARCH_VIRT_H_INCLUDED__
#define __ARCH_VIRT_H_INCLUDED__

#include <utils/utils.h>

uintptr_t arch_virt_new_root();
void arch_virt_free_root(uintptr_t root);
void arch_virt_set_root(uintptr_t root);
uintptr_t arch_virt_get_root();

extern uintptr_t ARCH_VIRT_KERNEL_MAPPING_BASE;
extern size_t ARCH_VIRT_PAGE_SIZE;

uintptr_t arch_virt_get_io_mapping(uintptr_t paddr, size_t size,
								   bool cache_disabled);

#endif
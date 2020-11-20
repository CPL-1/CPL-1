#ifndef __HAL_VIRT_H_INCLUDED__
#define __HAL_VIRT_H_INCLUDED__

#include <common/misc/utils.h>

uintptr_t hal_virt_new_root();
void hal_virt_free_root(uintptr_t root);
void hal_virt_set_root(uintptr_t root);
uintptr_t hal_virt_get_root();

extern uintptr_t hal_virt_kernel_mapping_base;
extern size_t hal_virt_page_size;

uintptr_t hal_virt_get_io_mapping(uintptr_t paddr, size_t size,
								  bool cache_disabled);

#endif
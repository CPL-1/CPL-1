#ifndef __HAL_VIRT_H_INCLUDED__
#define __HAL_VIRT_H_INCLUDED__

#include <common/misc/utils.h>

enum {
	HAL_VIRT_FLAGS_WRITABLE = 1,
	HAL_VIRT_FLAGS_EXECUTABLE = 2,
	HAL_VIRT_FLAGS_DISABLE_CACHE = 4,
	HAL_VIRT_FLAGS_USER_ACCESSIBLE = 8,
};

extern uintptr_t hal_virt_kernel_mapping_base;
extern uintptr_t hal_virt_user_area_start;
extern uintptr_t hal_virt_user_area_end;
extern size_t hal_virt_page_size;

uintptr_t hal_virt_new_root();
void hal_virt_free_root(uintptr_t root);
void hal_virt_set_root(uintptr_t root);
uintptr_t hal_virt_get_root();

bool hal_virt_map_page_at(uintptr_t root, uintptr_t vaddr, uintptr_t paddr,
						  int flags);

uintptr_t hal_virt_unmap_page_at(uintptr_t root, uintptr_t vaddr);

void hal_virt_change_perms(uintptr_t root, uintptr_t vaddr, int flags);

uintptr_t hal_virt_get_io_mapping(uintptr_t paddr, size_t size,
								  bool cache_disabled);

void hal_virt_flush();

#endif
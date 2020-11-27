#ifndef __VIRT_H_INCLUDED__
#define __VIRT_H_INLCUDED__

#include <common/core/proc/mutex.h>
#include <common/misc/utils.h>

struct virt_address_space {
	uintptr_t root;
	size_t ref_count;
	struct mutex mutex;
};

bool virt_map_at(struct virt_address_space *space, uintptr_t addr, size_t size,
				 int flags, bool lock);
void virt_unmap_at(struct virt_address_space *space, uintptr_t addr,
				   size_t size, bool lock);
struct virt_address_space *virt_make_address_space_from_root(uintptr_t root);

void virt_drop_address_space(struct virt_address_space *space);

struct virt_address_space *
virt_ref_address_space(struct virt_address_space *space);

struct virt_address_space *virt_new_address_space();

void virt_switch_to_address_space(struct virt_address_space *space);

#endif
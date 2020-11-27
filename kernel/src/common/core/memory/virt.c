#include <common/core/memory/heap.h>
#include <common/core/memory/virt.h>
#include <common/core/proc/proc.h>
#include <common/core/proc/proclayout.h>
#include <common/lib/kmsg.h>
#include <hal/memory/phys.h>
#include <hal/memory/virt.h>

#define VIRT_MOD_NAME "Virtual Memory Manager"

static struct virt_address_space *virt_get_current_address_space() {
	struct proc_id id = proc_my_id();
	struct proc_process *process = proc_get_data(id);
	if (process == NULL) {
		kmsg_err(VIRT_MOD_NAME, "Failed to get process data");
	}
	return process->address_space;
}

bool virt_map_at(struct virt_address_space *space, uintptr_t addr, size_t size,
				 int flags, bool lock) {
	struct virt_address_space *current_space = virt_get_current_address_space();
	if (space == NULL) {
		space = current_space;
	}
	if (lock) {
		mutex_lock(&(space->mutex));
	}
	for (uintptr_t current = addr; current < (addr + size);
		 current += hal_virt_page_size) {
		uintptr_t new_page = hal_phys_user_alloc_frame();
		if (new_page == 0) {
			goto failure;
		}
		if (!hal_virt_map_page_at(space->root, current, new_page, flags)) {
			hal_phys_user_free_frame(new_page);
			goto failure;
		}
		continue;
	failure:
		for (uintptr_t deallocating = addr; deallocating < current;
			 deallocating += hal_virt_page_size) {
			hal_virt_unmap_page_at(space->root, deallocating);
		}
		if (lock) {
			mutex_unlock(&(space->mutex));
		}
		return false;
	}
	if (lock) {
		mutex_unlock(&(space->mutex));
	}
	if (space == current_space) {
		hal_virt_flush();
	}
	return true;
}

void virt_unmap_at(struct virt_address_space *space, uintptr_t addr,
				   size_t size, bool lock) {
	struct virt_address_space *current_space = virt_get_current_address_space();
	if (space == NULL) {
		space = current_space;
	}
	if (lock) {
		mutex_lock(&(space->mutex));
	}
	for (uintptr_t current = addr; current < (addr + size);
		 current += hal_virt_page_size) {
		uintptr_t page = hal_virt_unmap_page_at(space->root, current);
		hal_phys_user_free_frame(page);
	}
	if (lock) {
		mutex_unlock(&(space->mutex));
	}
	if (space == current_space) {
		hal_virt_flush();
	}
}

struct virt_address_space *virt_make_address_space_from_root(uintptr_t root) {
	struct virt_address_space *space = ALLOC_OBJ(struct virt_address_space);
	if (space == NULL) {
		return NULL;
	}
	space->root = root;
	space->ref_count = 1;
	mutex_init(&(space->mutex));
	return space;
}

void virt_drop_address_space(struct virt_address_space *space) {
	if (space == NULL) {
		space = virt_get_current_address_space();
	}
	mutex_lock(&(space->mutex));
	if (space->ref_count == 0) {
		kmsg_err(VIRT_MOD_NAME, "Attempt to drop address space object when its "
								"reference count is already zero");
	}
	space->ref_count--;
	if (space->ref_count == 0) {
		mutex_unlock(&(space->mutex));
		hal_virt_free_root(space->root);
		FREE_OBJ(space);
		return;
	}
	mutex_unlock(&(space->mutex));
}

struct virt_address_space *
virt_ref_address_space(struct virt_address_space *space) {
	if (space == NULL) {
		space = virt_get_current_address_space();
	}
	mutex_lock(&(space->mutex));
	space->ref_count++;
	mutex_unlock(&(space->mutex));
	return space;
}

struct virt_address_space *virt_new_address_space() {
	uintptr_t new_root = hal_virt_new_root();
	if (new_root == 0) {
		return NULL;
	}
	struct virt_address_space *space =
		virt_make_address_space_from_root(new_root);
	if (space == NULL) {
		hal_virt_free_root(new_root);
	}
	return space;
}

void virt_switch_to_address_space(struct virt_address_space *space) {
	if (space == NULL) {
		space = virt_get_current_address_space();
	}
	hal_virt_set_root(space->root);
}
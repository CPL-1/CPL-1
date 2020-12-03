#ifndef __VIRT_H_INCLUDED__
#define __VIRT_H_INLCUDED__

#include <common/core/proc/mutex.h>
#include <common/lib/rbtree.h>
#include <common/misc/utils.h>

struct virt_memory_region_base {
	struct rb_node base;
	uintptr_t start;
	uintptr_t end;
	size_t size;
};

struct virt_memory_hole_node {
	struct virt_memory_region_base base;
	size_t max_size;
	size_t min_size;
	struct virt_memory_region_node *corresponding_region;
};

struct virt_memory_region_node {
	struct virt_memory_region_base base;
	struct virt_memory_hole_node *corresponding_hole;
	bool is_used;
};

struct virt_memory_regions_trees {
	struct rb_root holes_tree_root;
	struct rb_root regions_tree_root;
};

struct virt_address_space {
	uintptr_t root;
	size_t ref_count;
	struct mutex mutex;
	struct virt_memory_regions_trees trees;
};

uintptr_t virt_map_at(struct virt_address_space *space, uintptr_t addr,
					  size_t size, int flags, bool lock);
void virt_unmap_at(struct virt_address_space *space, uintptr_t addr,
				   size_t size, bool lock);
struct virt_address_space *virt_make_address_space_from_root(uintptr_t root);

void virt_drop_address_space(struct virt_address_space *space);

struct virt_address_space *
virt_ref_address_space(struct virt_address_space *space);

struct virt_address_space *virt_new_address_space();

void virt_switch_to_address_space(struct virt_address_space *space);

#endif
#include <common/core/memory/heap.h>
#include <common/core/memory/virt.h>
#include <common/core/proc/proc.h>
#include <common/core/proc/proclayout.h>
#include <common/lib/kmsg.h>
#include <hal/memory/phys.h>
#include <hal/memory/virt.h>

#define VIRT_MOD_NAME "Virtual Memory Manager"

static bool virt_update_max_and_min(struct virt_memory_hole_node *node) {
	struct virt_memory_hole_node *left_node =
		(struct virt_memory_hole_node *)(node->base.base.desc[0]);
	struct virt_memory_hole_node *right_node =
		(struct virt_memory_hole_node *)(node->base.base.desc[1]);
	size_t max_size = node->base.size;
	size_t min_size = node->base.size;
	bool updates_needed = false;
	if (left_node != NULL) {
		if (left_node->base.size < min_size) {
			min_size = left_node->base.size;
			updates_needed = true;
		} else if (left_node->base.size > max_size) {
			max_size = left_node->base.size;
			updates_needed = true;
		}
	}
	if (right_node != NULL) {
		if (right_node->base.size < min_size) {
			min_size = right_node->base.size;
			updates_needed = true;
		} else if (right_node->base.size > max_size) {
			max_size = right_node->base.size;
			updates_needed = true;
		}
	}
	node->max_size = max_size;
	node->min_size = min_size;
	return updates_needed;
}

static void virt_holes_augment_callback(struct rb_node *node) {
	while (node != NULL) {
		struct virt_memory_hole_node *data =
			(struct virt_memory_hole_node *)node;
		if (!virt_update_max_and_min(data)) {
			break;
		}
		node = node->parent;
	}
}

static int virt_find_best_fit_cmp(struct rb_node *desired,
								  struct rb_node *compared, unused void *ctx) {
	size_t size = ((struct virt_memory_hole_node *)desired)->base.size;
	struct virt_memory_hole_node *current_data =
		(struct virt_memory_hole_node *)(compared);
	struct virt_memory_hole_node *left_data =
		(struct virt_memory_hole_node *)(compared->desc[0]);
	if (left_data != NULL && left_data->max_size >= size) {
		return -1;
	} else if (current_data->max_size < size) {
		return 0;
	}
	return 1;
}

static int virt_holes_tree_insert_cmp(struct rb_node *left,
									  struct rb_node *right, void *ctx) {
	(void)ctx;
	struct virt_memory_hole_node *left_node =
		(struct virt_memory_hole_node *)left;
	struct virt_memory_hole_node *right_node =
		(struct virt_memory_hole_node *)right;
	return SPACESHIP_NO_ZERO(left_node->base.size, right_node->base.size);
}

static int virt_regions_get_at_start_cmp(struct rb_node *desired,
										 struct rb_node *compared, void *ctx) {
	(void)ctx;
	struct virt_memory_region_node *compared_node =
		(struct virt_memory_region_node *)compared;
	struct virt_memory_region_node *desired_node =
		(struct virt_memory_region_node *)desired;
	if (compared_node->base.start <= desired_node->base.start &&
		desired_node->base.start < compared_node->base.end) {
		return 0;
	}
	if (desired_node->base.start < compared_node->base.start) {
		return -1;
	}
	return 1;
}

void virt_initialize_regions_trees(struct virt_memory_regions_trees *regions) {
	regions->holes_tree_root.augment_callback = virt_holes_augment_callback;
	regions->holes_tree_root.root = NULL;
	regions->regions_tree_root.augment_callback = NULL;
	regions->regions_tree_root.root = NULL;
}

void virt_memory_region_cleanup_callback(struct rb_node *node) {
	struct virt_memory_region_node *region =
		(struct virt_memory_region_node *)node;
	FREE_OBJ(region);
}

void virt_memory_hole_cleanup_callback(struct rb_node *node) {
	struct virt_memory_hole_node *hole = (struct virt_memory_hole_node *)node;
	FREE_OBJ(hole);
}

void virt_cleanup_regions_trees(struct virt_memory_regions_trees *regions) {
	if (regions->holes_tree_root.root != NULL) {
		rb_clear(&(regions->holes_tree_root),
				 virt_memory_hole_cleanup_callback);
	}
	if (regions->regions_tree_root.root != NULL) {
		rb_clear(&(regions->holes_tree_root),
				 virt_memory_hole_cleanup_callback);
	}
}

bool virt_add_init_region(struct virt_memory_regions_trees *trees,
						  uintptr_t start, uintptr_t end) {
	struct virt_memory_hole_node *hole =
		ALLOC_OBJ(struct virt_memory_hole_node);
	if (hole == NULL) {
		return false;
	}
	struct virt_memory_region_node *region =
		ALLOC_OBJ(struct virt_memory_region_node);
	if (region == NULL) {
		FREE_OBJ(hole);
		return false;
	}
	hole->corresponding_region = region;
	region->corresponding_hole = hole;
	hole->base.start = start;
	hole->base.end = end;
	hole->base.size = hole->max_size = hole->min_size = end - start;
	region->base.start = start;
	region->base.end = end;
	region->base.size = end - start;
	region->is_used = false;
	rb_insert(&(trees->holes_tree_root), (struct rb_node *)hole,
			  virt_holes_tree_insert_cmp, NULL);
	rb_insert(&(trees->regions_tree_root), (struct rb_node *)region,
			  virt_regions_get_at_start_cmp, NULL);
	return true;
}

uintptr_t virt_allocate_region(struct virt_memory_regions_trees *trees,
							   size_t size) {
	struct virt_memory_hole_node query_node;
	query_node.base.size = size;
	struct virt_memory_hole_node *hole =
		(struct virt_memory_hole_node *)rb_query(
			&(trees->holes_tree_root), (struct rb_node *)&query_node,
			virt_find_best_fit_cmp, NULL, false);
	if (hole == NULL || hole->base.size < size) {
		return 0;
	}
	struct virt_memory_region_node *region = hole->corresponding_region;
	if (hole->base.size == size) {
		region->is_used = true;
		rb_delete(&(trees->holes_tree_root), (struct rb_node *)hole);
		FREE_OBJ(hole);
		return hole->base.start;
	}
	struct virt_memory_region_node *new_region =
		ALLOC_OBJ(struct virt_memory_region_node);
	if (new_region == NULL) {
		return 0;
	}
	rb_delete(&(trees->holes_tree_root), (struct rb_node *)hole);
	rb_delete(&(trees->regions_tree_root), (struct rb_node *)region);
	uintptr_t prev_region_end = region->base.end;
	region->base.end = region->base.start + size;
	region->base.size = region->base.end - region->base.start;
	new_region->base.start = region->base.end;
	new_region->base.end = prev_region_end;
	new_region->base.size = prev_region_end - region->base.end;
	region->corresponding_hole = NULL;
	region->is_used = true;
	new_region->is_used = false;
	new_region->corresponding_hole = hole;
	hole->corresponding_region = new_region;
	hole->base.start = new_region->base.start;
	hole->base.end = new_region->base.end;
	hole->base.size = new_region->base.size;
	hole->max_size = hole->min_size = hole->base.size;
	rb_insert(&(trees->holes_tree_root), (struct rb_node *)hole,
			  virt_holes_tree_insert_cmp, NULL);
	rb_insert(&(trees->regions_tree_root), (struct rb_node *)region,
			  virt_regions_get_at_start_cmp, NULL);
	rb_insert(&(trees->regions_tree_root), (struct rb_node *)new_region,
			  virt_regions_get_at_start_cmp, NULL);
	return region->base.start;
}

bool virt_reserve_region(struct virt_memory_regions_trees *trees,
						 uintptr_t start, uintptr_t end) {
	struct virt_memory_region_node query_node;
	query_node.base.start = start;
	struct virt_memory_region_node *region =
		(struct virt_memory_region_node *)rb_query(
			&(trees->regions_tree_root), (struct rb_node *)&query_node,
			virt_regions_get_at_start_cmp, NULL, false);
	if (region == NULL || region->is_used || end > region->base.end ||
		start < region->base.start) {
		return false;
	}
	struct virt_memory_hole_node *free_hole = region->corresponding_hole;
	struct virt_memory_region_node *left = NULL, *right = NULL;
	struct virt_memory_hole_node *left_hole = NULL, *right_hole = NULL;
	if (region->base.start < start) {
		left = ALLOC_OBJ(struct virt_memory_region_node);
		if (left == NULL) {
			return false;
		}
		left_hole = free_hole;
		free_hole = NULL;
	}
	if (region->base.end > end) {
		right = ALLOC_OBJ(struct virt_memory_region_node);
		if (right == NULL) {
			if (left != NULL) {
				FREE_OBJ(left);
			}
			return false;
		}
		if (free_hole != NULL) {
			right_hole = free_hole;
			free_hole = NULL;
		} else {
			right_hole = ALLOC_OBJ(struct virt_memory_hole_node);
			if (right_hole == NULL) {
				if (left != NULL) {
					FREE_OBJ(left);
				}
				FREE_OBJ(right);
				return false;
			}
		}
	}
	rb_delete(&(trees->holes_tree_root),
			  (struct rb_node *)(region->corresponding_hole));
	if (region->base.start == start && region->base.end == end) {
		region->is_used = true;
		region->corresponding_hole = NULL;
		FREE_OBJ(region->corresponding_hole);
		return true;
	}
	rb_delete(&(trees->regions_tree_root), (struct rb_node *)region);
	if (region->base.start < start) {
		left_hole->base.start = region->base.start;
		left_hole->base.end = start;
		left_hole->base.size = left_hole->base.end - left_hole->base.start;
		left_hole->max_size = left_hole->min_size = left_hole->base.size;
		left->base.end = left_hole->base.end;
		left->base.size = left_hole->base.size;
		left->base.start = left_hole->base.start;
		left->corresponding_hole = left_hole;
		left_hole->corresponding_region = left;
		left->is_used = false;
		rb_insert(&(trees->holes_tree_root), (struct rb_node *)left_hole,
				  virt_holes_tree_insert_cmp, NULL);
		rb_insert(&(trees->regions_tree_root), (struct rb_node *)left,
				  virt_regions_get_at_start_cmp, NULL);
	}
	if (region->base.end > end) {
		right_hole->base.end = region->base.end;
		right_hole->base.start = end;
		right_hole->base.size = right_hole->base.end - right_hole->base.start;
		right_hole->max_size = right_hole->min_size = right_hole->base.size;
		right->base.end = right_hole->base.end;
		right->base.size = right_hole->base.size;
		right->base.start = right_hole->base.start;
		right->corresponding_hole = right_hole;
		right_hole->corresponding_region = right;
		right->is_used = false;
		rb_insert(&(trees->holes_tree_root), (struct rb_node *)right_hole,
				  virt_holes_tree_insert_cmp, NULL);
		rb_insert(&(trees->regions_tree_root), (struct rb_node *)right,
				  virt_regions_get_at_start_cmp, NULL);
	}
	region->base.end = end;
	region->base.start = start;
	region->base.size = end - start;
	region->corresponding_hole = NULL;
	region->is_used = true;
	rb_insert(&(trees->regions_tree_root), (struct rb_node *)region,
			  virt_regions_get_at_start_cmp, NULL);
	return true;
}

bool virt_free_region(struct virt_memory_regions_trees *trees, uintptr_t start,
					  uintptr_t end) {
	if (start == end) {
		return true;
	}
	struct virt_memory_region_node query_node;
	query_node.base.start = start;
	struct virt_memory_region_node *region =
		(struct virt_memory_region_node *)rb_query(
			&(trees->regions_tree_root), (struct rb_node *)&query_node,
			virt_regions_get_at_start_cmp, NULL, false);
	if (region == NULL || !(region->is_used) || end > region->base.end ||
		start < region->base.start) {
		return false;
	}
	struct virt_memory_region_node *left_region = NULL, *right_region = NULL;
	struct virt_memory_hole_node *hole =
		ALLOC_OBJ(struct virt_memory_hole_node);
	if (hole == NULL) {
		return false;
	}
	if (region->base.start < start) {
		left_region = ALLOC_OBJ(struct virt_memory_region_node);
		if (left_region == NULL) {
			FREE_OBJ(hole);
			return false;
		}
	}
	if (region->base.end > end) {
		right_region = ALLOC_OBJ(struct virt_memory_region_node);
		if (right_region == NULL) {
			if (left_region != NULL) {
				FREE_OBJ(hole);
				FREE_OBJ(left_region);
			}
			return false;
		}
	}
	region->is_used = false;
	region->corresponding_hole = hole;
	hole->corresponding_region = region;
	struct virt_memory_region_node *left_adjoined_region = NULL,
								   *right_adjoined_region = NULL;
	if (region->base.start == start) {
		left_adjoined_region =
			(struct virt_memory_region_node *)region->base.base.iter[0];
		if (left_adjoined_region != NULL && left_adjoined_region->is_used) {
			left_adjoined_region = NULL;
		}
	}
	if (region->base.end == end) {
		right_adjoined_region =
			(struct virt_memory_region_node *)region->base.base.iter[1];
		if (right_adjoined_region != NULL && right_adjoined_region->is_used) {
			right_adjoined_region = NULL;
		}
	}
	rb_delete(&(trees->regions_tree_root), (struct rb_node *)region);
	if (left_adjoined_region != NULL) {
		region->base.start = left_adjoined_region->base.start;
		region->base.size = region->base.end - region->base.start;
		rb_delete(&(trees->regions_tree_root),
				  (struct rb_node *)(left_adjoined_region));
		rb_delete(&(trees->holes_tree_root),
				  (struct rb_node *)(left_adjoined_region->corresponding_hole));
		FREE_OBJ(left_adjoined_region->corresponding_hole);
		FREE_OBJ(left_adjoined_region);
	} else if (left_region != NULL) {
		left_region->is_used = true;
		left_region->corresponding_hole = NULL;
		left_region->base.start = region->base.start;
		left_region->base.end = start;
		left_region->base.size = left_region->base.end - left_region->base.size;
		region->base.start = left_region->base.end;
		region->base.size = region->base.end - region->base.start;
		rb_insert(&(trees->regions_tree_root), (struct rb_node *)left_region,
				  virt_regions_get_at_start_cmp, NULL);
	}
	if (right_adjoined_region != NULL) {
		region->base.end = right_adjoined_region->base.end;
		region->base.size = region->base.end - region->base.start;
		rb_delete(&(trees->regions_tree_root),
				  (struct rb_node *)right_adjoined_region);
		rb_delete(
			&(trees->holes_tree_root),
			(struct rb_node *)(right_adjoined_region->corresponding_hole));
		FREE_OBJ(right_adjoined_region->corresponding_hole);
		FREE_OBJ(right_adjoined_region);
	} else if (right_region != NULL) {
		right_region->is_used = true;
		right_region->corresponding_hole = NULL;
		right_region->base.start = end;
		right_region->base.end = region->base.end;
		right_region->base.size =
			right_region->base.end - right_region->base.start;
		region->base.end = end;
		region->base.size = region->base.end - region->base.start;
		rb_insert(&(trees->regions_tree_root), (struct rb_node *)right_region,
				  virt_regions_get_at_start_cmp, NULL);
	}
	hole->base.end = region->base.end;
	hole->base.size = region->base.size;
	hole->base.start = region->base.start;
	hole->max_size = hole->min_size = hole->base.size;
	rb_insert(&(trees->regions_tree_root), (struct rb_node *)region,
			  virt_regions_get_at_start_cmp, NULL);
	rb_insert(&(trees->holes_tree_root), (struct rb_node *)hole,
			  virt_holes_tree_insert_cmp, NULL);
	return true;
}

static struct virt_address_space *virt_get_current_address_space() {
	struct proc_id id = proc_my_id();
	struct proc_process *process = proc_get_data(id);
	if (process == NULL) {
		kmsg_err(VIRT_MOD_NAME, "Failed to get process data");
	}
	return process->address_space;
}

uintptr_t virt_map_at(struct virt_address_space *space, uintptr_t addr,
					  size_t size, int flags, bool lock) {
	struct virt_address_space *current_space = virt_get_current_address_space();
	if (space == NULL) {
		space = current_space;
	}
	if (lock) {
		mutex_lock(&(space->mutex));
	}
	if (addr == 0) {
		addr = virt_allocate_region(&(space->trees), size);
		if (addr == 0) {
			return 0;
		}
	} else if (!virt_reserve_region(&(space->trees), addr, addr + size)) {
		return 0;
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
		// TODO: do something better than leaking virtual address space
		// in case of failure
		virt_free_region(&(space->trees), addr, addr + size);
		return false;
	}
	if (lock) {
		mutex_unlock(&(space->mutex));
	}
	if (space == current_space) {
		hal_virt_flush();
	}
	return addr;
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
	// TODO: do something better than leaking virtual address space
	// in case of failure
	if (!virt_free_region(&(space->trees), addr, addr + size)) {
		return;
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
		virt_cleanup_regions_trees(&(space->trees));
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
	virt_initialize_regions_trees(&(space->trees));
	if (!virt_add_init_region(&(space->trees), hal_virt_user_area_start,
							  hal_virt_user_area_end)) {
		hal_virt_free_root(new_root);
		FREE_OBJ(space);
		return NULL;
	}
	return space;
}

void virt_switch_to_address_space(struct virt_address_space *space) {
	if (space == NULL) {
		space = virt_get_current_address_space();
	}
	hal_virt_set_root(space->root);
}
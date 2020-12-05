#include <common/core/memory/heap.h>
#include <common/core/memory/virt.h>
#include <common/core/proc/proc.h>
#include <common/core/proc/proclayout.h>
#include <common/lib/kmsg.h>
#include <hal/memory/phys.h>
#include <hal/memory/virt.h>

#define VIRT_MOD_NAME "Virtual Memory Manager"

static bool VirtualMM_UpdateMaxAndMinHoleSizes(struct VirtualMM_MemoryHoleNode *node) {
	struct VirtualMM_MemoryHoleNode *leftNode = (struct VirtualMM_MemoryHoleNode *)(node->base.base.desc[0]);
	struct VirtualMM_MemoryHoleNode *rightNode = (struct VirtualMM_MemoryHoleNode *)(node->base.base.desc[1]);
	size_t maxSize = node->base.size;
	size_t minSize = node->base.size;
	bool updatesNeeded = false;
	if (leftNode != NULL) {
		if (leftNode->base.size < minSize) {
			minSize = leftNode->base.size;
			updatesNeeded = true;
		} else if (leftNode->base.size > maxSize) {
			maxSize = leftNode->base.size;
			updatesNeeded = true;
		}
	}
	if (rightNode != NULL) {
		if (rightNode->base.size < minSize) {
			minSize = rightNode->base.size;
			updatesNeeded = true;
		} else if (rightNode->base.size > maxSize) {
			maxSize = rightNode->base.size;
			updatesNeeded = true;
		}
	}
	node->maxSize = maxSize;
	node->minSize = minSize;
	return updatesNeeded;
}

static void VirtualMM_HolesAugmentCallback(struct RedBlackTree_Node *node) {
	while (node != NULL) {
		struct VirtualMM_MemoryHoleNode *data = (struct VirtualMM_MemoryHoleNode *)node;
		if (!VirtualMM_UpdateMaxAndMinHoleSizes(data)) {
			break;
		}
		node = node->parent;
	}
}

static int VirtualMM_FindBestFitComparator(struct RedBlackTree_Node *desired, struct RedBlackTree_Node *compared,
										   UNUSED void *ctx) {
	size_t size = ((struct VirtualMM_MemoryHoleNode *)desired)->base.size;
	struct VirtualMM_MemoryHoleNode *currentData = (struct VirtualMM_MemoryHoleNode *)(compared);
	struct VirtualMM_MemoryHoleNode *leftData = (struct VirtualMM_MemoryHoleNode *)(compared->desc[0]);
	if (leftData != NULL && leftData->maxSize >= size) {
		return -1;
	} else if (currentData->maxSize < size) {
		return 0;
	}
	return 1;
}

static int VirtualMM_HolesTreeInsertionComparator(struct RedBlackTree_Node *left, struct RedBlackTree_Node *right,
												  void *ctx) {
	(void)ctx;
	struct VirtualMM_MemoryHoleNode *leftNode = (struct VirtualMM_MemoryHoleNode *)left;
	struct VirtualMM_MemoryHoleNode *rightNode = (struct VirtualMM_MemoryHoleNode *)right;
	return SPACESHIP_NO_ZERO(leftNode->base.size, rightNode->base.size);
}

static int VirtualMM_GetMemoryAreaComparator(struct RedBlackTree_Node *desired, struct RedBlackTree_Node *compared,
											 void *ctx) {
	(void)ctx;
	struct VirtualMM_MemoryRegionNode *comparedNode = (struct VirtualMM_MemoryRegionNode *)compared;
	struct VirtualMM_MemoryRegionNode *desiredNode = (struct VirtualMM_MemoryRegionNode *)desired;
	if (comparedNode->base.start <= desiredNode->base.start && desiredNode->base.start < comparedNode->base.end) {
		return 0;
	}
	if (desiredNode->base.start < comparedNode->base.start) {
		return -1;
	}
	return 1;
}

void VirtualMM_InitializeRegionTrees(struct VirtualMM_RegionTrees *regions) {
	regions->holesTreeRoot.augmentCallback = VirtualMM_HolesAugmentCallback;
	regions->holesTreeRoot.root = NULL;
	regions->regionsTreeRoot.augmentCallback = NULL;
	regions->regionsTreeRoot.root = NULL;
}

void VirtualMM_FreeMemoryRegionNode(struct RedBlackTree_Node *node) {
	struct VirtualMM_MemoryRegionNode *region = (struct VirtualMM_MemoryRegionNode *)node;
	FREE_OBJ(region);
}

void VirtualMM_FreeMemoryHoleNode(struct RedBlackTree_Node *node) {
	struct VirtualMM_MemoryHoleNode *hole = (struct VirtualMM_MemoryHoleNode *)node;
	FREE_OBJ(hole);
}

void VirtualMM_CleanupRegionTrees(struct VirtualMM_RegionTrees *regions) {
	if (regions->holesTreeRoot.root != NULL) {
		RedBlackTree_Clear(&(regions->holesTreeRoot), VirtualMM_FreeMemoryHoleNode);
	}
	if (regions->regionsTreeRoot.root != NULL) {
		RedBlackTree_Clear(&(regions->holesTreeRoot), VirtualMM_FreeMemoryHoleNode);
	}
}

bool VirtualMM_AddInitRegion(struct VirtualMM_RegionTrees *trees, uintptr_t start, uintptr_t end) {
	struct VirtualMM_MemoryHoleNode *hole = ALLOC_OBJ(struct VirtualMM_MemoryHoleNode);
	if (hole == NULL) {
		return false;
	}
	struct VirtualMM_MemoryRegionNode *region = ALLOC_OBJ(struct VirtualMM_MemoryRegionNode);
	if (region == NULL) {
		FREE_OBJ(hole);
		return false;
	}
	hole->correspondingRegion = region;
	region->correspondingHole = hole;
	hole->base.start = start;
	hole->base.end = end;
	hole->base.size = hole->maxSize = hole->minSize = end - start;
	region->base.start = start;
	region->base.end = end;
	region->base.size = end - start;
	region->isUsed = false;
	RedBlackTree_Insert(&(trees->holesTreeRoot), (struct RedBlackTree_Node *)hole,
						VirtualMM_HolesTreeInsertionComparator, NULL);
	RedBlackTree_Insert(&(trees->regionsTreeRoot), (struct RedBlackTree_Node *)region,
						VirtualMM_GetMemoryAreaComparator, NULL);
	return true;
}

uintptr_t VirtualMM_AllocateRegion(struct VirtualMM_RegionTrees *trees, size_t size) {
	struct VirtualMM_MemoryHoleNode queryNode;
	queryNode.base.size = size;
	struct VirtualMM_MemoryHoleNode *hole = (struct VirtualMM_MemoryHoleNode *)RedBlackTree_Query(
		&(trees->holesTreeRoot), (struct RedBlackTree_Node *)&queryNode, VirtualMM_FindBestFitComparator, NULL, false);
	if (hole == NULL || hole->base.size < size) {
		return 0;
	}
	struct VirtualMM_MemoryRegionNode *region = hole->correspondingRegion;
	if (hole->base.size == size) {
		region->isUsed = true;
		RedBlackTree_Remove(&(trees->holesTreeRoot), (struct RedBlackTree_Node *)hole);
		FREE_OBJ(hole);
		return hole->base.start;
	}
	struct VirtualMM_MemoryRegionNode *newRegion = ALLOC_OBJ(struct VirtualMM_MemoryRegionNode);
	if (newRegion == NULL) {
		return 0;
	}
	RedBlackTree_Remove(&(trees->holesTreeRoot), (struct RedBlackTree_Node *)hole);
	RedBlackTree_Remove(&(trees->regionsTreeRoot), (struct RedBlackTree_Node *)region);
	uintptr_t prevRegionEnd = region->base.end;
	region->base.end = region->base.start + size;
	region->base.size = region->base.end - region->base.start;
	newRegion->base.start = region->base.end;
	newRegion->base.end = prevRegionEnd;
	newRegion->base.size = prevRegionEnd - region->base.end;
	region->correspondingHole = NULL;
	region->isUsed = true;
	newRegion->isUsed = false;
	newRegion->correspondingHole = hole;
	hole->correspondingRegion = newRegion;
	hole->base.start = newRegion->base.start;
	hole->base.end = newRegion->base.end;
	hole->base.size = newRegion->base.size;
	hole->maxSize = hole->minSize = hole->base.size;
	RedBlackTree_Insert(&(trees->holesTreeRoot), (struct RedBlackTree_Node *)hole,
						VirtualMM_HolesTreeInsertionComparator, NULL);
	RedBlackTree_Insert(&(trees->regionsTreeRoot), (struct RedBlackTree_Node *)region,
						VirtualMM_GetMemoryAreaComparator, NULL);
	RedBlackTree_Insert(&(trees->regionsTreeRoot), (struct RedBlackTree_Node *)newRegion,
						VirtualMM_GetMemoryAreaComparator, NULL);
	return region->base.start;
}

bool VirtualMM_ReserveRegion(struct VirtualMM_RegionTrees *trees, uintptr_t start, uintptr_t end) {
	struct VirtualMM_MemoryRegionNode queryNode;
	queryNode.base.start = start;
	struct VirtualMM_MemoryRegionNode *region = (struct VirtualMM_MemoryRegionNode *)RedBlackTree_Query(
		&(trees->regionsTreeRoot), (struct RedBlackTree_Node *)&queryNode, VirtualMM_GetMemoryAreaComparator, NULL,
		false);
	if (region == NULL || region->isUsed || end > region->base.end || start < region->base.start) {
		return false;
	}
	struct VirtualMM_MemoryHoleNode *freeHole = region->correspondingHole;
	struct VirtualMM_MemoryRegionNode *left = NULL, *right = NULL;
	struct VirtualMM_MemoryHoleNode *leftHole = NULL, *rightHole = NULL;
	if (region->base.start < start) {
		left = ALLOC_OBJ(struct VirtualMM_MemoryRegionNode);
		if (left == NULL) {
			return false;
		}
		leftHole = freeHole;
		freeHole = NULL;
	}
	if (region->base.end > end) {
		right = ALLOC_OBJ(struct VirtualMM_MemoryRegionNode);
		if (right == NULL) {
			if (left != NULL) {
				FREE_OBJ(left);
			}
			return false;
		}
		if (freeHole != NULL) {
			rightHole = freeHole;
			freeHole = NULL;
		} else {
			rightHole = ALLOC_OBJ(struct VirtualMM_MemoryHoleNode);
			if (rightHole == NULL) {
				if (left != NULL) {
					FREE_OBJ(left);
				}
				FREE_OBJ(right);
				return false;
			}
		}
	}
	RedBlackTree_Remove(&(trees->holesTreeRoot), (struct RedBlackTree_Node *)(region->correspondingHole));
	if (region->base.start == start && region->base.end == end) {
		region->isUsed = true;
		region->correspondingHole = NULL;
		FREE_OBJ(region->correspondingHole);
		return true;
	}
	RedBlackTree_Remove(&(trees->regionsTreeRoot), (struct RedBlackTree_Node *)region);
	if (region->base.start < start) {
		leftHole->base.start = region->base.start;
		leftHole->base.end = start;
		leftHole->base.size = leftHole->base.end - leftHole->base.start;
		leftHole->maxSize = leftHole->minSize = leftHole->base.size;
		left->base.end = leftHole->base.end;
		left->base.size = leftHole->base.size;
		left->base.start = leftHole->base.start;
		left->correspondingHole = leftHole;
		leftHole->correspondingRegion = left;
		left->isUsed = false;
		RedBlackTree_Insert(&(trees->holesTreeRoot), (struct RedBlackTree_Node *)leftHole,
							VirtualMM_HolesTreeInsertionComparator, NULL);
		RedBlackTree_Insert(&(trees->regionsTreeRoot), (struct RedBlackTree_Node *)left,
							VirtualMM_GetMemoryAreaComparator, NULL);
	}
	if (region->base.end > end) {
		rightHole->base.end = region->base.end;
		rightHole->base.start = end;
		rightHole->base.size = rightHole->base.end - rightHole->base.start;
		rightHole->maxSize = rightHole->minSize = rightHole->base.size;
		right->base.end = rightHole->base.end;
		right->base.size = rightHole->base.size;
		right->base.start = rightHole->base.start;
		right->correspondingHole = rightHole;
		rightHole->correspondingRegion = right;
		right->isUsed = false;
		RedBlackTree_Insert(&(trees->holesTreeRoot), (struct RedBlackTree_Node *)rightHole,
							VirtualMM_HolesTreeInsertionComparator, NULL);
		RedBlackTree_Insert(&(trees->regionsTreeRoot), (struct RedBlackTree_Node *)right,
							VirtualMM_GetMemoryAreaComparator, NULL);
	}
	region->base.end = end;
	region->base.start = start;
	region->base.size = end - start;
	region->correspondingHole = NULL;
	region->isUsed = true;
	RedBlackTree_Insert(&(trees->regionsTreeRoot), (struct RedBlackTree_Node *)region,
						VirtualMM_GetMemoryAreaComparator, NULL);
	return true;
}

bool VirtualMM_FreeRegion(struct VirtualMM_RegionTrees *trees, uintptr_t start, uintptr_t end) {
	if (start == end) {
		return true;
	}
	struct VirtualMM_MemoryRegionNode queryNode;
	queryNode.base.start = start;
	struct VirtualMM_MemoryRegionNode *region = (struct VirtualMM_MemoryRegionNode *)RedBlackTree_Query(
		&(trees->regionsTreeRoot), (struct RedBlackTree_Node *)&queryNode, VirtualMM_GetMemoryAreaComparator, NULL,
		false);
	if (region == NULL || !(region->isUsed) || end > region->base.end || start < region->base.start) {
		return false;
	}
	struct VirtualMM_MemoryRegionNode *leftRegion = NULL, *rightRegion = NULL;
	struct VirtualMM_MemoryHoleNode *hole = ALLOC_OBJ(struct VirtualMM_MemoryHoleNode);
	if (hole == NULL) {
		return false;
	}
	if (region->base.start < start) {
		leftRegion = ALLOC_OBJ(struct VirtualMM_MemoryRegionNode);
		if (leftRegion == NULL) {
			FREE_OBJ(hole);
			return false;
		}
	}
	if (region->base.end > end) {
		rightRegion = ALLOC_OBJ(struct VirtualMM_MemoryRegionNode);
		if (rightRegion == NULL) {
			if (leftRegion != NULL) {
				FREE_OBJ(hole);
				FREE_OBJ(leftRegion);
			}
			return false;
		}
	}
	region->isUsed = false;
	region->correspondingHole = hole;
	hole->correspondingRegion = region;
	struct VirtualMM_MemoryRegionNode *leftAdjoinedRegion = NULL, *rightAdjoinedRegion = NULL;
	if (region->base.start == start) {
		leftAdjoinedRegion = (struct VirtualMM_MemoryRegionNode *)region->base.base.iter[0];
		if (leftAdjoinedRegion != NULL && leftAdjoinedRegion->isUsed) {
			leftAdjoinedRegion = NULL;
		}
	}
	if (region->base.end == end) {
		rightAdjoinedRegion = (struct VirtualMM_MemoryRegionNode *)region->base.base.iter[1];
		if (rightAdjoinedRegion != NULL && rightAdjoinedRegion->isUsed) {
			rightAdjoinedRegion = NULL;
		}
	}
	RedBlackTree_Remove(&(trees->regionsTreeRoot), (struct RedBlackTree_Node *)region);
	if (leftAdjoinedRegion != NULL) {
		region->base.start = leftAdjoinedRegion->base.start;
		region->base.size = region->base.end - region->base.start;
		RedBlackTree_Remove(&(trees->regionsTreeRoot), (struct RedBlackTree_Node *)(leftAdjoinedRegion));
		RedBlackTree_Remove(&(trees->holesTreeRoot),
							(struct RedBlackTree_Node *)(leftAdjoinedRegion->correspondingHole));
		FREE_OBJ(leftAdjoinedRegion->correspondingHole);
		FREE_OBJ(leftAdjoinedRegion);
	} else if (leftRegion != NULL) {
		leftRegion->isUsed = true;
		leftRegion->correspondingHole = NULL;
		leftRegion->base.start = region->base.start;
		leftRegion->base.end = start;
		leftRegion->base.size = leftRegion->base.end - leftRegion->base.size;
		region->base.start = leftRegion->base.end;
		region->base.size = region->base.end - region->base.start;
		RedBlackTree_Insert(&(trees->regionsTreeRoot), (struct RedBlackTree_Node *)leftRegion,
							VirtualMM_GetMemoryAreaComparator, NULL);
	}
	if (rightAdjoinedRegion != NULL) {
		region->base.end = rightAdjoinedRegion->base.end;
		region->base.size = region->base.end - region->base.start;
		RedBlackTree_Remove(&(trees->regionsTreeRoot), (struct RedBlackTree_Node *)rightAdjoinedRegion);
		RedBlackTree_Remove(&(trees->holesTreeRoot),
							(struct RedBlackTree_Node *)(rightAdjoinedRegion->correspondingHole));
		FREE_OBJ(rightAdjoinedRegion->correspondingHole);
		FREE_OBJ(rightAdjoinedRegion);
	} else if (rightRegion != NULL) {
		rightRegion->isUsed = true;
		rightRegion->correspondingHole = NULL;
		rightRegion->base.start = end;
		rightRegion->base.end = region->base.end;
		rightRegion->base.size = rightRegion->base.end - rightRegion->base.start;
		region->base.end = end;
		region->base.size = region->base.end - region->base.start;
		RedBlackTree_Insert(&(trees->regionsTreeRoot), (struct RedBlackTree_Node *)rightRegion,
							VirtualMM_GetMemoryAreaComparator, NULL);
	}
	hole->base.end = region->base.end;
	hole->base.size = region->base.size;
	hole->base.start = region->base.start;
	hole->maxSize = hole->minSize = hole->base.size;
	RedBlackTree_Insert(&(trees->regionsTreeRoot), (struct RedBlackTree_Node *)region,
						VirtualMM_GetMemoryAreaComparator, NULL);
	RedBlackTree_Insert(&(trees->holesTreeRoot), (struct RedBlackTree_Node *)hole,
						VirtualMM_HolesTreeInsertionComparator, NULL);
	return true;
}

static struct VirtualMM_AddressSpace *VirtualMM_GetCurrentAddressSpace() {
	struct Proc_ProcessID id = Proc_GetProcessID();
	struct Proc_Process *process = Proc_GetProcessData(id);
	if (process == NULL) {
		KernelLog_ErrorMsg(VIRT_MOD_NAME, "Failed to get process data");
	}
	return process->address_space;
}

uintptr_t VirtualMM_MemoryMap(struct VirtualMM_AddressSpace *space, uintptr_t addr, size_t size, int flags, bool lock) {
	struct VirtualMM_AddressSpace *currentSpace = VirtualMM_GetCurrentAddressSpace();
	if (space == NULL) {
		space = currentSpace;
	}
	if (lock) {
		Mutex_Lock(&(space->mutex));
	}
	if (addr == 0) {
		addr = VirtualMM_AllocateRegion(&(space->trees), size);
		if (addr == 0) {
			return 0;
		}
	} else if (!VirtualMM_ReserveRegion(&(space->trees), addr, addr + size)) {
		return 0;
	}
	for (uintptr_t current = addr; current < (addr + size); current += HAL_VirtualMM_PageSize) {
		uintptr_t new_page = HAL_PhysicalMM_UserAllocFrame();
		if (new_page == 0) {
			goto failure;
		}
		if (!HAL_VirtualMM_MapPageAt(space->root, current, new_page, flags)) {
			HAL_PhysicalMM_UserFreeFrame(new_page);
			goto failure;
		}
		continue;
	failure:
		for (uintptr_t deallocating = addr; deallocating < current; deallocating += HAL_VirtualMM_PageSize) {
			HAL_VirtualMM_UnmapPageAt(space->root, deallocating);
		}
		if (lock) {
			Mutex_Unlock(&(space->mutex));
		}
		// TODO: do something better than leaking virtual address space
		// in case of failure
		VirtualMM_FreeRegion(&(space->trees), addr, addr + size);
		return false;
	}
	if (lock) {
		Mutex_Unlock(&(space->mutex));
	}
	if (space == currentSpace) {
		HAL_VirtualMM_Flush();
	}
	return addr;
}

void VirtualMM_MemoryUnmap(struct VirtualMM_AddressSpace *space, uintptr_t addr, size_t size, bool lock) {
	struct VirtualMM_AddressSpace *currentSpace = VirtualMM_GetCurrentAddressSpace();
	if (space == NULL) {
		space = currentSpace;
	}
	if (lock) {
		Mutex_Lock(&(space->mutex));
	}
	// TODO: do something better than leaking virtual address space
	// in case of failure
	VirtualMM_FreeRegion(&(space->trees), addr, addr + size);
	for (uintptr_t current = addr; current < (addr + size); current += HAL_VirtualMM_PageSize) {
		uintptr_t page = HAL_VirtualMM_UnmapPageAt(space->root, current);
		HAL_PhysicalMM_UserFreeFrame(page);
	}
	if (lock) {
		Mutex_Unlock(&(space->mutex));
	}
	if (space == currentSpace) {
		HAL_VirtualMM_Flush();
	}
}

void VirtualMM_MemoryRetype(struct VirtualMM_AddressSpace *space, uintptr_t addr, size_t size, int flags, bool lock) {
	struct VirtualMM_AddressSpace *currentSpace = VirtualMM_GetCurrentAddressSpace();
	if (space == NULL) {
		space = currentSpace;
	}
	if (lock) {
		Mutex_Lock(&(space->mutex));
	}
	for (uintptr_t current = addr; current < (addr + size); current += HAL_VirtualMM_PageSize) {
		HAL_VirtualMM_ChangePagePermissions(space->root, current, flags);
	}
	if (lock) {
		Mutex_Unlock(&(space->mutex));
	}
	if (space == currentSpace) {
		HAL_VirtualMM_Flush();
	}
}

struct VirtualMM_AddressSpace *VirtualMM_MakeAddressSpaceFromRoot(uintptr_t root) {
	struct VirtualMM_AddressSpace *space = ALLOC_OBJ(struct VirtualMM_AddressSpace);
	if (space == NULL) {
		return NULL;
	}
	space->root = root;
	space->refCount = 1;
	Mutex_Initialize(&(space->mutex));
	return space;
}

void VirtualMM_DropAddressSpace(struct VirtualMM_AddressSpace *space) {
	if (space == NULL) {
		space = VirtualMM_GetCurrentAddressSpace();
	}
	Mutex_Lock(&(space->mutex));
	if (space->refCount == 0) {
		KernelLog_ErrorMsg(VIRT_MOD_NAME, "Attempt to drop address space object when its "
										  "reference count is already zero");
	}
	space->refCount--;
	if (space->refCount == 0) {
		Mutex_Unlock(&(space->mutex));
		HAL_VirtualMM_FreeAddressSpace(space->root);
		VirtualMM_CleanupRegionTrees(&(space->trees));
		FREE_OBJ(space);
		return;
	}
	Mutex_Unlock(&(space->mutex));
}

struct VirtualMM_AddressSpace *VirtualMM_ReferenceAddressSpace(struct VirtualMM_AddressSpace *space) {
	if (space == NULL) {
		space = VirtualMM_GetCurrentAddressSpace();
	}
	Mutex_Lock(&(space->mutex));
	space->refCount++;
	Mutex_Unlock(&(space->mutex));
	return space;
}

struct VirtualMM_AddressSpace *VirtualMM_MakeNewAddressSpace() {
	uintptr_t newRoot = HAL_VirtualMM_MakeNewAddressSpace();
	if (newRoot == 0) {
		return NULL;
	}
	struct VirtualMM_AddressSpace *space = VirtualMM_MakeAddressSpaceFromRoot(newRoot);
	if (space == NULL) {
		HAL_VirtualMM_FreeAddressSpace(newRoot);
	}
	VirtualMM_InitializeRegionTrees(&(space->trees));
	if (!VirtualMM_AddInitRegion(&(space->trees), HAL_VirtualMM_UserAreaStart, HAL_VirtualMM_UserAreaEnd)) {
		HAL_VirtualMM_FreeAddressSpace(newRoot);
		FREE_OBJ(space);
		return NULL;
	}
	return space;
}

void VirtualMM_SwitchToAddressSpace(struct VirtualMM_AddressSpace *space) {
	if (space == NULL) {
		space = VirtualMM_GetCurrentAddressSpace();
	}
	HAL_VirtualMM_SwitchToAddressSpace(space->root);
}
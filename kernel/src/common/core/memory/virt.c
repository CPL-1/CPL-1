#include <common/core/memory/heap.h>
#include <common/core/memory/virt.h>
#include <common/core/proc/proc.h>
#include <common/core/proc/proclayout.h>
#include <common/lib/kmsg.h>
#include <hal/memory/phys.h>
#include <hal/memory/virt.h>
#include <hal/proc/intlevel.h>

#define VIRT_MOD_NAME "Virtual Memory Manager"

static bool VirtualMM_EnoughMemFilter(struct RedBlackTree_Node *node, void *ctx) {
	size_t size = *(size_t *)ctx;
	return ((struct VirtualMM_MemoryRegionBase *)node)->size >= size;
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
	regions->holesTreeRoot.root = NULL;
	regions->regionsTreeRoot.root = NULL;
}

void VirtualMM_FreeMemoryRegionNode(struct RedBlackTree_Node *node, void *opaque) {
	struct VirtualMM_AddressSpace *space = (struct VirtualMM_AddressSpace *)opaque;
	struct VirtualMM_MemoryRegionNode *region = (struct VirtualMM_MemoryRegionNode *)node;
	if (region->isUsed) {
		for (uintptr_t current = region->base.start; current < region->base.end; current += HAL_VirtualMM_PageSize) {
			uintptr_t page = HAL_VirtualMM_UnmapPageAt(space->root, current);
			if (page != 0) {
				HAL_PhysicalMM_UserFreeFrame(page);
			}
		}
	}
	FREE_OBJ(region);
}

void VirtualMM_FreeMemoryHoleNode(struct RedBlackTree_Node *node, MAYBE_UNUSED void *opaque) {
	struct VirtualMM_MemoryHoleNode *hole = (struct VirtualMM_MemoryHoleNode *)node;
	FREE_OBJ(hole);
}

void VirtualMM_CleanupRegionTrees(struct VirtualMM_AddressSpace *space) {
	if (space->trees.holesTreeRoot.root != NULL) {
		RedBlackTree_Clear(&(space->trees.holesTreeRoot), VirtualMM_FreeMemoryHoleNode, (void *)space);
	}
	if (space->trees.regionsTreeRoot.root != NULL) {
		RedBlackTree_Clear(&(space->trees.regionsTreeRoot), VirtualMM_FreeMemoryRegionNode, (void *)space);
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
	hole->base.size = end - start;
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

struct VirtualMM_MemoryRegionNode *VirtualMM_AllocateRegion(struct VirtualMM_RegionTrees *trees, size_t size,
															int flags) {
	size_t sizeBuf = size;
	struct VirtualMM_MemoryHoleNode *hole = (struct VirtualMM_MemoryHoleNode *)RedBlackTree_LowerBound(
		&(trees->holesTreeRoot), VirtualMM_EnoughMemFilter, &sizeBuf);
	if (hole == NULL || hole->base.size < size) {
		return NULL;
	}
	struct VirtualMM_MemoryRegionNode *region = hole->correspondingRegion;
	if (hole->base.size == size) {
		region->isUsed = true;
		RedBlackTree_Remove(&(trees->holesTreeRoot), (struct RedBlackTree_Node *)hole);
		FREE_OBJ(hole);
		return hole->correspondingRegion;
	}
	struct VirtualMM_MemoryRegionNode *newRegion = ALLOC_OBJ(struct VirtualMM_MemoryRegionNode);
	if (newRegion == NULL) {
		return NULL;
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
	newRegion->flags = flags;
	hole->correspondingRegion = newRegion;
	hole->base.start = newRegion->base.start;
	hole->base.end = newRegion->base.end;
	hole->base.size = newRegion->base.size;
	RedBlackTree_Insert(&(trees->holesTreeRoot), (struct RedBlackTree_Node *)hole,
						VirtualMM_HolesTreeInsertionComparator, NULL);
	RedBlackTree_Insert(&(trees->regionsTreeRoot), (struct RedBlackTree_Node *)region,
						VirtualMM_GetMemoryAreaComparator, NULL);
	RedBlackTree_Insert(&(trees->regionsTreeRoot), (struct RedBlackTree_Node *)newRegion,
						VirtualMM_GetMemoryAreaComparator, NULL);
	return region;
}

struct VirtualMM_MemoryRegionNode *VirtualMM_MemoryGetRegionByAddress(struct VirtualMM_RegionTrees *trees,
																	  uintptr_t addr) {
	struct VirtualMM_MemoryRegionNode queryNode;
	queryNode.base.start = addr;
	struct VirtualMM_MemoryRegionNode *region = (struct VirtualMM_MemoryRegionNode *)RedBlackTree_Query(
		&(trees->regionsTreeRoot), (struct RedBlackTree_Node *)&queryNode, VirtualMM_GetMemoryAreaComparator, NULL,
		false);
	return region;
}

struct VirtualMM_MemoryRegionNode *VirtualMM_ReserveRegion(struct VirtualMM_RegionTrees *trees, uintptr_t start,
														   uintptr_t end, int flags) {
	struct VirtualMM_MemoryRegionNode *region = VirtualMM_MemoryGetRegionByAddress(trees, start);
	if (region == NULL || region->isUsed || end > region->base.end || start < region->base.start) {
		return NULL;
	}
	struct VirtualMM_MemoryHoleNode *freeHole = region->correspondingHole;
	struct VirtualMM_MemoryRegionNode *left = NULL, *right = NULL;
	struct VirtualMM_MemoryHoleNode *leftHole = NULL, *rightHole = NULL;
	if (region->base.start < start) {
		left = ALLOC_OBJ(struct VirtualMM_MemoryRegionNode);
		if (left == NULL) {
			return NULL;
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
			return NULL;
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
				return NULL;
			}
		}
	}
	RedBlackTree_Remove(&(trees->holesTreeRoot), (struct RedBlackTree_Node *)(region->correspondingHole));
	if (region->base.start == start && region->base.end == end) {
		region->isUsed = true;
		region->correspondingHole = NULL;
		FREE_OBJ(region->correspondingHole);
		return region;
	}
	RedBlackTree_Remove(&(trees->regionsTreeRoot), (struct RedBlackTree_Node *)region);
	if (region->base.start < start) {
		leftHole->base.start = region->base.start;
		leftHole->base.end = start;
		leftHole->base.size = leftHole->base.end - leftHole->base.start;
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
	region->flags = flags;
	RedBlackTree_Insert(&(trees->regionsTreeRoot), (struct RedBlackTree_Node *)region,
						VirtualMM_GetMemoryAreaComparator, NULL);
	return region;
}

enum {
	VIRTUALMM_FREE_REGION_SUCCESS,
	VIRTUALMM_FREE_REGION_ERROR,
	VIRTUALMM_FREE_ALLOCATION_FAILURE
} VirtualMM_FreeRegion(struct VirtualMM_RegionTrees *trees, uintptr_t start, uintptr_t end) {
	if (start == end) {
		return VIRTUALMM_FREE_REGION_SUCCESS;
	}
	struct VirtualMM_MemoryRegionNode *region = VirtualMM_MemoryGetRegionByAddress(trees, start);
	if (region == NULL || !(region->isUsed) || end > region->base.end || start < region->base.start) {
		return VIRTUALMM_FREE_REGION_ERROR;
	}
	struct VirtualMM_MemoryRegionNode *leftRegion = NULL, *rightRegion = NULL;
	struct VirtualMM_MemoryHoleNode *hole = ALLOC_OBJ(struct VirtualMM_MemoryHoleNode);
	if (hole == NULL) {
		return VIRTUALMM_FREE_REGION_ERROR;
	}
	if (region->base.start < start) {
		leftRegion = ALLOC_OBJ(struct VirtualMM_MemoryRegionNode);
		if (leftRegion == NULL) {
			FREE_OBJ(hole);
			return VIRTUALMM_FREE_ALLOCATION_FAILURE;
		}
	}
	if (region->base.end > end) {
		rightRegion = ALLOC_OBJ(struct VirtualMM_MemoryRegionNode);
		if (rightRegion == NULL) {
			if (leftRegion != NULL) {
				FREE_OBJ(hole);
				FREE_OBJ(leftRegion);
			}
			return VIRTUALMM_FREE_ALLOCATION_FAILURE;
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
	RedBlackTree_Insert(&(trees->regionsTreeRoot), (struct RedBlackTree_Node *)region,
						VirtualMM_GetMemoryAreaComparator, NULL);
	RedBlackTree_Insert(&(trees->holesTreeRoot), (struct RedBlackTree_Node *)hole,
						VirtualMM_HolesTreeInsertionComparator, NULL);
	return VIRTUALMM_FREE_REGION_SUCCESS;
}

struct VirtualMM_AddressSpace *VirtualMM_GetCurrentAddressSpace() {
	struct Proc_ProcessID id = Proc_GetProcessID();
	struct Proc_Process *process = Proc_GetProcessData(id);
	if (process == NULL) {
		KernelLog_ErrorMsg(VIRT_MOD_NAME, "Failed to get process data");
	}
	return process->addressSpace;
}

struct VirtualMM_MemoryRegionNode *VirtualMM_MemoryMap(struct VirtualMM_AddressSpace *space, uintptr_t addr,
													   size_t size, int flags, bool lock) {
	struct VirtualMM_AddressSpace *currentSpace = VirtualMM_GetCurrentAddressSpace();
	if (space == NULL) {
		space = currentSpace;
	}
	if (lock) {
		Mutex_Lock(&(space->mutex));
	}
	struct VirtualMM_MemoryRegionNode *node;
	if (addr == 0) {
		node = VirtualMM_AllocateRegion(&(space->trees), size, flags);
	} else {
		node = VirtualMM_ReserveRegion(&(space->trees), addr, addr + size, flags);
	}
	if (node == NULL) {
		KernelLog_ErrorMsg("Virt", "Alloc node failure");
		return NULL;
	}
	addr = node->base.start;
	for (uintptr_t current = addr; current < (addr + size); current += HAL_VirtualMM_PageSize) {
		uintptr_t new_page = HAL_PhysicalMM_UserAllocFrame();
		if (new_page == 0) {
			KernelLog_ErrorMsg("Virt", "Phys Alloc failure");
			goto failure;
		}
		if (!HAL_VirtualMM_MapPageAt(space->root, current, new_page, flags)) {
			KernelLog_ErrorMsg("Virt", "Mapping failure");
			HAL_PhysicalMM_UserFreeFrame(new_page);
			goto failure;
		}
		continue;
	failure:
		for (uintptr_t deallocating = addr; deallocating < current; deallocating += HAL_VirtualMM_PageSize) {
			uintptr_t result = HAL_VirtualMM_UnmapPageAt(space->root, deallocating);
			if (result != 0) {
				HAL_PhysicalMM_UserFreeFrame(deallocating);
			}
		}
		if (lock) {
			Mutex_Unlock(&(space->mutex));
		}
		// TODO: do something better than leaking virtual address space
		// in case of failure
		VirtualMM_FreeRegion(&(space->trees), addr, addr + size);
		return NULL;
	}
	node->flags = flags;
	if (space == currentSpace) {
		HAL_VirtualMM_Flush();
	}
	if (lock) {
		Mutex_Unlock(&(space->mutex));
	}
	return node;
}

int VirtualMM_MemoryUnmap(struct VirtualMM_AddressSpace *space, uintptr_t addr, size_t size, bool lock) {
	struct VirtualMM_AddressSpace *currentSpace = VirtualMM_GetCurrentAddressSpace();
	if (space == NULL) {
		space = currentSpace;
	}
	if (lock) {
		Mutex_Lock(&(space->mutex));
	}
	// TODO: do something better than leaking virtual address space
	// in case of failure
	int status = VirtualMM_FreeRegion(&(space->trees), addr, addr + size);
	if (status == VIRTUALMM_FREE_REGION_ERROR) {
		return -1;
	}
	for (uintptr_t current = addr; current < (addr + size); current += HAL_VirtualMM_PageSize) {
		uintptr_t page = HAL_VirtualMM_UnmapPageAt(space->root, current);
		if (page != 0) {
			HAL_PhysicalMM_UserFreeFrame(page);
		}
	}
	if (lock) {
		Mutex_Unlock(&(space->mutex));
	}
	if (space == currentSpace) {
		HAL_VirtualMM_Flush();
	}
	return 0;
}

void VirtualMM_MemoryRetype(struct VirtualMM_AddressSpace *space, struct VirtualMM_MemoryRegionNode *region,
							int flags) {
	struct VirtualMM_AddressSpace *currentSpace = VirtualMM_GetCurrentAddressSpace();
	if (space == NULL) {
		space = currentSpace;
	}
	region->flags = flags;
	for (uintptr_t current = region->base.start; current < region->base.end; current += HAL_VirtualMM_PageSize) {
		HAL_VirtualMM_ChangePagePermissions(space->root, current, flags);
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
	VirtualMM_InitializeRegionTrees(&(space->trees));
	if (!VirtualMM_AddInitRegion(&(space->trees), HAL_VirtualMM_UserAreaStart, HAL_VirtualMM_UserAreaEnd)) {
		FREE_OBJ(space);
		return NULL;
	}
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
		VirtualMM_CleanupRegionTrees(space);
		HAL_VirtualMM_FreeAddressSpace(space->root);
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
	struct Proc_ProcessID id = Proc_GetProcessID();
	struct Proc_Process *process = Proc_GetProcessData(id);
	if (process == NULL) {
		KernelLog_ErrorMsg(VIRT_MOD_NAME, "Failed to get process data");
	}
	int level = HAL_InterruptLevel_Elevate();
	process->addressSpace = space;
	HAL_VirtualMM_SwitchToAddressSpace(space->root);
	HAL_InterruptLevel_Recover(level);
}

void VirtualMM_PreemptToAddressSpace(struct VirtualMM_AddressSpace *space) {
	if (space == NULL) {
		space = VirtualMM_GetCurrentAddressSpace();
	}
	HAL_VirtualMM_PreemptToAddressSpace(space->root);
}

static void VirtualMM_CopyPageAcrossAddessSpaces(struct VirtualMM_AddressSpace *src, struct VirtualMM_AddressSpace *dst,
												 char *buf, uintptr_t addr, uintptr_t size) {

	memcpy((void *)buf, (void *)addr, size);
	VirtualMM_SwitchToAddressSpace(dst);
	memcpy((void *)addr, (void *)buf, size);
	VirtualMM_SwitchToAddressSpace(src);
}

#define VIRTUALMM_COPY_BUFFER_SIZE 0x100000

static void VirtualMM_CopyPagesAcrossAddressSpaces(struct VirtualMM_AddressSpace *src,
												   struct VirtualMM_AddressSpace *dst, char *buf, uintptr_t start,
												   uintptr_t end) {
	for (uintptr_t addr = start; addr < end; addr += VIRTUALMM_COPY_BUFFER_SIZE) {
		size_t size = VIRTUALMM_COPY_BUFFER_SIZE;
		if ((end - addr) < size) {
			size = end - addr;
		}
		VirtualMM_CopyPageAcrossAddessSpaces(src, dst, buf, addr, size);
	}
}

struct VirtualMM_AddressSpace *VirtualMM_CopyCurrentAddressSpace() {
	char *copyBuffer = Heap_AllocateMemory(VIRTUALMM_COPY_BUFFER_SIZE);
	if (copyBuffer == NULL) {
		return NULL;
	}
	struct VirtualMM_AddressSpace *newSpace = VirtualMM_MakeNewAddressSpace();
	if (newSpace == NULL) {
		Heap_FreeMemory(copyBuffer, VIRTUALMM_COPY_BUFFER_SIZE);
		return NULL;
	}
	struct VirtualMM_AddressSpace *currentSpace = VirtualMM_GetCurrentAddressSpace();
	Mutex_Lock(&(currentSpace->mutex));
	struct RedBlackTree_Node *current = currentSpace->trees.regionsTreeRoot.ends[0];
	while (current != NULL) {
		struct VirtualMM_MemoryRegionNode *region = (struct VirtualMM_MemoryRegionNode *)current;
		if (region->isUsed) {
			if (!VirtualMM_MemoryMap(newSpace, region->base.start, region->base.size,
									 HAL_VIRT_FLAGS_WRITABLE | HAL_VIRT_FLAGS_READABLE, false)) {
				Heap_FreeMemory(copyBuffer, VIRTUALMM_COPY_BUFFER_SIZE);
				VirtualMM_DropAddressSpace(newSpace);
				return NULL;
			}
		}
		current = current->iter[1];
	}
	current = currentSpace->trees.regionsTreeRoot.ends[0];
	while (current != NULL) {
		struct VirtualMM_MemoryRegionNode *region = (struct VirtualMM_MemoryRegionNode *)current;
		if (region->isUsed) {
			int oldType = region->flags;
			VirtualMM_MemoryRetype(currentSpace, region, HAL_VIRT_FLAGS_WRITABLE | HAL_VIRT_FLAGS_READABLE);
			VirtualMM_CopyPagesAcrossAddressSpaces(currentSpace, newSpace, copyBuffer, region->base.start,
												   region->base.end);
			VirtualMM_MemoryRetype(currentSpace, region, oldType);
			VirtualMM_MemoryRetype(newSpace, region, region->flags);
		}
		current = current->iter[1];
	}
	Heap_FreeMemory(copyBuffer, VIRTUALMM_COPY_BUFFER_SIZE);
	Mutex_Unlock(&(currentSpace->mutex));
	return newSpace;
}

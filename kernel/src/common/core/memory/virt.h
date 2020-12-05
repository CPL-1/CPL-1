#ifndef __VIRT_H_INCLUDED__
#define __VIRT_H_i686_Ports_ReadDoubleWordCUDED__

#include <common/core/proc/mutex.h>
#include <common/lib/rbtree.h>
#include <common/misc/utils.h>

struct VirtualMM_MemoryRegionBase {
	struct RedBlackTree_Node base;
	UINTN start;
	UINTN end;
	USIZE size;
};

struct VirtualMM_MemoryHoleNode {
	struct VirtualMM_MemoryRegionBase base;
	USIZE maxSize;
	USIZE minSize;
	struct VirtualMM_MemoryRegionNode *correspondingRegion;
};

struct VirtualMM_MemoryRegionNode {
	struct VirtualMM_MemoryRegionBase base;
	struct VirtualMM_MemoryHoleNode *correspondingHole;
	bool isUsed;
};

struct VirtualMM_RegionTrees {
	struct RedBlackTree_Tree holesTreeRoot;
	struct RedBlackTree_Tree regionsTreeRoot;
};

struct VirtualMM_AddressSpace {
	UINTN root;
	USIZE refCount;
	struct Mutex mutex;
	struct VirtualMM_RegionTrees trees;
};

UINTN VirtualMM_MemoryMap(struct VirtualMM_AddressSpace *space, UINTN addr, USIZE size, int flags, bool lock);
void VirtualMM_MemoryUnmap(struct VirtualMM_AddressSpace *space, UINTN addr, USIZE size, bool lock);
struct VirtualMM_AddressSpace *VirtualMM_MakeAddressSpaceFromRoot(UINTN root);
void VirtualMM_DropAddressSpace(struct VirtualMM_AddressSpace *space);
struct VirtualMM_AddressSpace *VirtualMM_ReferenceAddressSpace(struct VirtualMM_AddressSpace *space);
struct VirtualMM_AddressSpace *VirtualMM_MakeNewAddressSpace();
void VirtualMM_SwitchToAddressSpace(struct VirtualMM_AddressSpace *space);

#endif
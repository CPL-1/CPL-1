#include <common/core/memory/heap.h>
#include <common/core/memory/iomap.h>
#include <common/core/proc/mutex.h>
#include <common/lib/kmsg.h>
#include <common/lib/rbtree.h>
#include <hal/memory/phys.h>
#include <hal/memory/virt.h>

#define IOMAP_MOD_NAME "IO Mapping Manager"

struct IOMap_MemoryRegion {
	struct RedBlackTree_Node base;
	uintptr_t start;
	size_t size;
};

static struct RedBlackTree_Tree m_regions;
static struct Mutex m_mutex;

static bool IOMap_EnoughMemFilter(struct RedBlackTree_Node *node, void *ctx) {
	size_t size = *(size_t *)ctx;
	return ((struct IOMap_MemoryRegion *)node)->size >= size;
}

static int IOMap_InsertionComparator(struct RedBlackTree_Node *left, struct RedBlackTree_Node *right,
									 MAYBE_UNUSED void *ctx) {
	struct IOMap_MemoryRegion *leftNode = (struct IOMap_MemoryRegion *)left;
	struct IOMap_MemoryRegion *rightNode = (struct IOMap_MemoryRegion *)right;
	return SPACESHIP_NO_ZERO(leftNode->size, rightNode->size);
}

void IOMap_Initialize() {
	RedBlackTree_Initialize(&(m_regions));
	struct IOMap_MemoryRegion *region = ALLOC_OBJ(struct IOMap_MemoryRegion);
	region->start = HAL_VirtualMM_IOMappingsStart;
	region->size = HAL_VirtualMM_IOMappingsEnd - HAL_VirtualMM_IOMappingsStart;
	RedBlackTree_Insert(&m_regions, (struct RedBlackTree_Node *)region, IOMap_InsertionComparator, NULL);
	Mutex_Initialize(&m_mutex);
}

static uintptr_t IOMap_AllocateIOMemoryRegion(size_t size) {
	struct IOMap_MemoryRegion *region =
		(struct IOMap_MemoryRegion *)RedBlackTree_LowerBound(&m_regions, IOMap_EnoughMemFilter, &size);
	if (region == NULL) {
		return 0;
	}
	RedBlackTree_Remove(&m_regions, (struct RedBlackTree_Node *)region);
	uintptr_t result = region->start;
	if (region->size == size) {
		FREE_OBJ(region);
		return result;
	}
	region->start += size;
	region->size -= size;
	RedBlackTree_Insert(&m_regions, (struct RedBlackTree_Node *)region, IOMap_InsertionComparator, NULL);
	return result;
}

static void IOMap_FreeIOMemoryRegion(uintptr_t start, size_t size) {
	struct IOMap_MemoryRegion *region = ALLOC_OBJ(struct IOMap_MemoryRegion);
	if (region == NULL) {
		KernelLog_WarnMsg(IOMAP_MOD_NAME, "Failed to allocate node for freed IO region");
		return;
	}
	region->start = start;
	region->size = size;
	RedBlackTree_Insert(&m_regions, (struct RedBlackTree_Node *)region, IOMap_InsertionComparator, NULL);
}

uintptr_t IOMap_AllocateIOMapping(uintptr_t paddr, size_t size, bool cacheDisabled) {
	uintptr_t vspace = HAL_VirtualMM_GetCurrentAddressSpace();
	uintptr_t vaddr = IOMap_AllocateIOMemoryRegion(size);
	if (vaddr == 0) {
		Mutex_Unlock(&m_mutex);
		return 0;
	}
	for (size_t offset = 0; offset < size; offset += HAL_VirtualMM_PageSize) {
		if (!HAL_VirtualMM_MapPageAt(vspace, vaddr + offset, paddr + offset,
									 HAL_VIRT_FLAGS_READABLE | HAL_VIRT_FLAGS_WRITABLE |
										 (cacheDisabled ? HAL_VIRT_FLAGS_DISABLE_CACHE : 0))) {
			for (size_t offset2 = 0; offset2 < offset; offset2 += HAL_VirtualMM_PageSize) {
				HAL_VirtualMM_UnmapPageAt(vspace, vaddr + offset2);
			}
			Mutex_Unlock(&m_mutex);
			return 0;
		}
	}
	HAL_VirtualMM_Flush();
	Mutex_Unlock(&m_mutex);
	return vaddr;
}

void IOMap_FreeIOMapping(uintptr_t vaddr, size_t size) {
	uintptr_t vspace = HAL_VirtualMM_GetCurrentAddressSpace();
	Mutex_Lock(&m_mutex);
	for (size_t offset = 0; offset < size; offset += HAL_VirtualMM_PageSize) {
		HAL_VirtualMM_UnmapPageAt(vspace, vaddr + offset);
	}
	IOMap_FreeIOMemoryRegion(vaddr, size);
	Mutex_Unlock(&m_mutex);
}

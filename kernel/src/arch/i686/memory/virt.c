#include <arch/i686/cpu/cr3.h>
#include <arch/i686/memory/config.h>
#include <arch/i686/memory/phys.h>
#include <arch/i686/memory/virt.h>
#include <arch/i686/proc/priv.h>
#include <common/lib/kmsg.h>
#include <hal/memory/phys.h>
#include <hal/memory/virt.h>

#define I686_VIRT_MOD_NAME "i686 Virtual Memory Manager"
#define I686_FLAGS_MASK 0b111111111111

union i686_VirtualMM_PageTableEntry {
	UINT32 addr;
	struct {
		UINT32 present : 1;
		UINT32 writable : 1;
		UINT32 user : 1;
		UINT32 writeThrough : 1;
		UINT32 cacheDisabled : 1;
		UINT32 accessed : 1;
		UINT32 huge : 1;
		UINT32 ignored : 1;
		UINT32 : 24;
	} PACKED;
} PACKED;

struct i686_VirtualMM_PageTable {
	union i686_VirtualMM_PageTableEntry entries[1024];
} PACKED;

UINTN HAL_VirtualMM_KernelMappingBase = I686_KERNEL_MAPPING_BASE;
USIZE HAL_VirtualMM_PageSize = I686_PAGE_SIZE;
UINTN HAL_VirtualMM_UserAreaStart = I686_USER_AREA_START;
UINTN HAL_VirtualMM_UserAreaEnd = I686_USER_AREA_END;
UINT16 *i686_VirtualMM_PageRefcounts = NULL;

static INLINE UINT16 i686_VirtualMM_GetPageDirectoryIndex(UINT32 vaddr) {
	return (vaddr >> 22) & (0b1111111111);
}

static INLINE UINT16 i686_VirtualMM_GetPageTableIndex(UINT32 vaddr) {
	return (vaddr >> 12) & (0b1111111111);
}

static INLINE UINT32 i686_VirtualMM_WalkToNextPageTable(UINT32 current, UINT16 index) {
	struct i686_VirtualMM_PageTable *table = (struct i686_VirtualMM_PageTable *)(current + I686_KERNEL_MAPPING_BASE);
	if (!table->entries[index].present) {
		return 0;
	}
	return table->entries[index].addr & ~(I686_FLAGS_MASK);
}

static void i686_VirtualMM_CreateBootstrapMapping(UINT32 cr3, UINT32 vaddr, UINT32 paddr) {
	UINT16 pdIndex = i686_VirtualMM_GetPageDirectoryIndex(vaddr);
	UINT16 ptIndex = i686_VirtualMM_GetPageTableIndex(vaddr);
	struct i686_VirtualMM_PageTable *pageDir = (struct i686_VirtualMM_PageTable *)(cr3 + I686_KERNEL_MAPPING_BASE);
	if (!(pageDir->entries[pdIndex].present)) {
		UINT32 addr = i686_PhysicalMM_KernelAllocFrame();
		if (addr == 0) {
			KernelLog_ErrorMsg(I686_VIRT_MOD_NAME, "Failed to allocate page table for the kernel memory mapping");
		}
		if (addr > I686_KERNEL_INIT_MAPPING_SIZE) {
			KernelLog_ErrorMsg(I686_VIRT_MOD_NAME, "Allocated page table is not reachable from the initial "
												   "kernel mapping");
		}
		pageDir->entries[pdIndex].addr = addr;
		pageDir->entries[pdIndex].present = true;
		pageDir->entries[pdIndex].writable = true;
	}
	struct i686_VirtualMM_PageTable *pageTable =
		(struct i686_VirtualMM_PageTable *)(i686_VirtualMM_WalkToNextPageTable(cr3, pdIndex) +
											I686_KERNEL_MAPPING_BASE);
	pageTable->entries[ptIndex].addr = paddr;
	pageTable->entries[ptIndex].present = true;
	pageTable->entries[ptIndex].writable = true;
}

static UINT16 i686_VirtualMM_GetPageTableReferenceCount(UINT32 paddr) {
	struct i686_VirtualMM_PageTable *pageTable = (struct i686_VirtualMM_PageTable *)(paddr + I686_KERNEL_MAPPING_BASE);
	UINT16 result = 0;
	for (UINT16 i = 0; i < 1024; ++i) {
		if (pageTable->entries[i].present) {
			result++;
		}
	}
	return result;
}

void i686_VirtualMM_InitializeKernelMap() {
	UINT32 cr3 = i686_CR3_Get();
	for (UINT32 paddr = I686_KERNEL_INIT_MAPPING_SIZE; paddr < I686_PHYS_LOW_LIMIT; paddr += I686_PAGE_SIZE) {
		i686_VirtualMM_CreateBootstrapMapping(cr3, paddr + I686_KERNEL_MAPPING_BASE, paddr);
	}
	struct i686_VirtualMM_PageTable *pageDir = (struct i686_VirtualMM_PageTable *)(cr3 + I686_KERNEL_MAPPING_BASE);
	pageDir->entries[0].addr = 0;
	i686_CPU_SetCR3(i686_CPU_GetCR3());
	UINT32 refcountsSize = i686_PhysicalMM_GetMemorySize() * 2 / HAL_VirtualMM_PageSize;
	UINT32 refcounts = HAL_PhysicalMM_KernelAllocArea(refcountsSize);
	if (refcounts == 0) {
		KernelLog_ErrorMsg(I686_VIRT_MOD_NAME, "Failed to allocate frame refcount array");
	}
	i686_VirtualMM_PageRefcounts = (UINT16 *)(refcounts + HAL_VirtualMM_KernelMappingBase);
	memset(i686_VirtualMM_PageRefcounts, 0, refcountsSize);
	for (UINT16 i = 0; i < 1024; ++i) {
		if (pageDir->entries[i].present) {
			UINT32 next = i686_VirtualMM_WalkToNextPageTable(cr3, i);
			UINT16 refcount = i686_VirtualMM_GetPageTableReferenceCount(next);
			i686_VirtualMM_PageRefcounts[next / HAL_VirtualMM_PageSize] = refcount;
		}
	}
}

bool HAL_VirtualMM_MapPageAt(UINTN root, UINTN vaddr, UINTN paddr, int flags) {
	struct i686_VirtualMM_PageTable *pageDir = (struct i686_VirtualMM_PageTable *)(root + I686_KERNEL_MAPPING_BASE);
	UINT16 pdIndex = i686_VirtualMM_GetPageDirectoryIndex(vaddr);
	UINT16 ptIndex = i686_VirtualMM_GetPageTableIndex(vaddr);
	if (!(pageDir->entries[pdIndex].present)) {
		UINT32 addr = i686_PhysicalMM_KernelAllocFrame();
		if (addr == 0) {
			return false;
		}
		pageDir->entries[pdIndex].addr = addr;
		pageDir->entries[pdIndex].present = true;
		pageDir->entries[pdIndex].writable = true;
		pageDir->entries[pdIndex].user = true;
		i686_VirtualMM_PageRefcounts[addr / HAL_VirtualMM_PageSize] = 0;
		memset((void *)(addr + HAL_VirtualMM_KernelMappingBase), 0, 0x1000);
	}
	UINT32 next = i686_VirtualMM_WalkToNextPageTable(root, pdIndex);
	struct i686_VirtualMM_PageTable *pageTable = (struct i686_VirtualMM_PageTable *)(next + I686_KERNEL_MAPPING_BASE);
	if (pageTable->entries[ptIndex].present) {
		KernelLog_ErrorMsg(I686_VIRT_MOD_NAME, "Mapping over already mapped page is not allowed");
	}
	pageTable->entries[ptIndex].addr = paddr;
	pageTable->entries[ptIndex].present = true;
	pageTable->entries[ptIndex].writable = (flags & HAL_VIRT_FLAGS_WRITABLE) != 0;
	pageTable->entries[ptIndex].cacheDisabled = (flags & HAL_VIRT_FLAGS_DISABLE_CACHE) != 0;
	pageTable->entries[ptIndex].user = (flags & HAL_VIRT_FLAGS_USER_ACCESSIBLE) != 0;
	i686_VirtualMM_PageRefcounts[next / HAL_VirtualMM_PageSize]++;
	return true;
}

UINTN HAL_VirtualMM_UnmapPageAt(UINTN root, UINTN vaddr) {
	UINT16 pdIndex = i686_VirtualMM_GetPageDirectoryIndex(vaddr);
	UINT16 ptIndex = i686_VirtualMM_GetPageTableIndex(vaddr);
	UINT32 pageTablePhys = i686_VirtualMM_WalkToNextPageTable(root, pdIndex);
	if (pageTablePhys == 0) {
		KernelLog_ErrorMsg(I686_VIRT_MOD_NAME, "Trying to unmap virtual page page "
											   "table for which is not mapped");
	}
	struct i686_VirtualMM_PageTable *pageDir = (struct i686_VirtualMM_PageTable *)(root + I686_KERNEL_MAPPING_BASE);
	struct i686_VirtualMM_PageTable *pageTable =
		(struct i686_VirtualMM_PageTable *)(pageTablePhys + I686_KERNEL_MAPPING_BASE);
	if (!(pageTable->entries[ptIndex].present)) {
		KernelLog_ErrorMsg(I686_VIRT_MOD_NAME, "Trying to unmap virtual page page which is not mapped");
	}
	UINTN result = i686_VirtualMM_WalkToNextPageTable(pageTablePhys, ptIndex);
	pageTable->entries[ptIndex].addr = 0;
	if (i686_VirtualMM_PageRefcounts[pageTablePhys / HAL_VirtualMM_PageSize] == 0) {
		KernelLog_ErrorMsg(I686_VIRT_MOD_NAME, "Attempt to decrement reference count which is already zero");
	}
	--i686_VirtualMM_PageRefcounts[pageTablePhys / HAL_VirtualMM_PageSize];
	if (i686_VirtualMM_PageRefcounts[pageTablePhys / HAL_VirtualMM_PageSize] == 0) {
		pageDir->entries[pdIndex].addr = 0;
		HAL_PhysicalMM_KernelFreeFrame(pageTablePhys);
	}
	return result;
}

void HAL_VirtualMM_ChangePagePermissions(UINTN root, UINTN vaddr, int flags) {
	UINT16 pdIndex = i686_VirtualMM_GetPageDirectoryIndex(vaddr);
	UINT16 ptIndex = i686_VirtualMM_GetPageTableIndex(vaddr);
	UINT32 pageTablePhys = i686_VirtualMM_WalkToNextPageTable(root, pdIndex);
	if (pageTablePhys == 0) {
		KernelLog_ErrorMsg(I686_VIRT_MOD_NAME, "Trying to change permissions on the page, page table for "
											   "which is not mapped");
	}
	struct i686_VirtualMM_PageTable *pageTable =
		(struct i686_VirtualMM_PageTable *)(pageTablePhys + I686_KERNEL_MAPPING_BASE);
	UINTN addr = i686_VirtualMM_WalkToNextPageTable(pageTablePhys, ptIndex);
	if (!(pageTable->entries[ptIndex].present)) {
		KernelLog_ErrorMsg(I686_VIRT_MOD_NAME, "Trying to unmap virtual page page which is not mapped");
	}
	pageTable->entries[ptIndex].addr = addr;
	pageTable->entries[ptIndex].writable = (flags & HAL_VIRT_FLAGS_WRITABLE) != 0;
	pageTable->entries[ptIndex].cacheDisabled = (flags & HAL_VIRT_FLAGS_DISABLE_CACHE) != 0;
	pageTable->entries[ptIndex].user = (flags & HAL_VIRT_FLAGS_USER_ACCESSIBLE) != 0;
}

UINTN HAL_VirtualMM_MakeNewAddressSpace() {
	UINT32 frame = i686_PhysicalMM_KernelAllocFrame();
	if (frame == 0) {
		return 0;
	}
	UINT32 *writableFrame = (UINT32 *)(frame + I686_KERNEL_MAPPING_BASE);
	UINT32 *pageDir = (UINT32 *)(i686_CR3_Get() + I686_KERNEL_MAPPING_BASE);
	memset(writableFrame, 0, I686_PAGE_SIZE);
	memcpy(writableFrame + 768, pageDir + 768, (1024 - 768) * 4);
	return frame;
}

void HAL_VirtualMM_FreeAddressSpace(UINTN root) {
	HAL_PhysicalMM_KernelFreeFrame(root);
}

void HAL_VirtualMM_SwitchToAddressSpace(UINTN root) {
	i686_CR3_Set(root);
}

UINTN HAL_VirtualMM_GetCurrentAddressSpace(UINTN root) {
	return i686_CR3_Get(root);
}

static void i686_VirtualMM_MapPageInKernelSpace(UINT32 vaddr, UINT32 paddr, bool cacheDisabled) {
	UINT32 pdIndex = i686_VirtualMM_GetPageDirectoryIndex(vaddr);
	UINT32 ptIndex = i686_VirtualMM_GetPageTableIndex(vaddr);
	UINT32 cr3 = i686_CR3_Get();
	UINT32 pageTableAddr = i686_VirtualMM_WalkToNextPageTable(cr3, pdIndex);
	struct i686_VirtualMM_PageTable *table =
		(struct i686_VirtualMM_PageTable *)(pageTableAddr + I686_KERNEL_MAPPING_BASE);
	table->entries[ptIndex].addr = paddr;
	table->entries[ptIndex].present = true;
	table->entries[ptIndex].writable = true;
	table->entries[ptIndex].writeThrough = true;
	table->entries[ptIndex].cacheDisabled = cacheDisabled;
}

static void i686_VirtualMM_FlushCR3FromRing0() {
	i686_CR3_Set(i686_CR3_Get());
}

static void i686_VirtualMM_FlushCR3() {
	i686_Ring0Executor_Invoke((int)i686_VirtualMM_FlushCR3FromRing0, 0);
}

UINTN HAL_VirtualMM_GetIOMapping(UINTN paddr, USIZE size, bool cacheDisabled) {
	UINT32 area = HAL_PhysicalMM_KernelAllocArea(size);
	if (area == 0) {
		return 0;
	}
	for (UINT32 offset = 0; offset < size; offset += I686_PAGE_SIZE) {
		i686_VirtualMM_MapPageInKernelSpace(I686_KERNEL_MAPPING_BASE + area + offset, paddr + offset, cacheDisabled);
	}
	i686_VirtualMM_FlushCR3();
	return area + I686_KERNEL_MAPPING_BASE;
}

void HAL_VirtualMM_Flush() {
	i686_VirtualMM_FlushCR3();
}
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
	uint32_t addr;
	struct {
		uint32_t present : 1;
		uint32_t writable : 1;
		uint32_t user : 1;
		uint32_t writeThrough : 1;
		uint32_t cacheDisabled : 1;
		uint32_t accessed : 1;
		uint32_t huge : 1;
		uint32_t ignored : 1;
		uint32_t : 24;
	} PACKED;
} PACKED;

struct i686_VirtualMM_PageTable {
	union i686_VirtualMM_PageTableEntry entries[1024];
} PACKED;

uintptr_t HAL_VirtualMM_KernelMappingBase = I686_KERNEL_MAPPING_BASE;
size_t HAL_VirtualMM_PageSize = I686_PAGE_SIZE;
uintptr_t HAL_VirtualMM_UserAreaStart = I686_USER_AREA_START;
uintptr_t HAL_VirtualMM_UserAreaEnd = I686_USER_AREA_END;
uint16_t *m_pageRefcounts = NULL;

static INLINE uint16_t i686_VirtualMM_GetPageDirectoryIndex(uint32_t vaddr) {
	return (vaddr >> 22) & (0b1111111111);
}

static INLINE uint16_t i686_VirtualMM_GetPageTableIndex(uint32_t vaddr) {
	return (vaddr >> 12) & (0b1111111111);
}

static INLINE uint32_t i686_VirtualMM_WalkToNextPageTable(uint32_t current, uint16_t index) {
	struct i686_VirtualMM_PageTable *table = (struct i686_VirtualMM_PageTable *)(current + I686_KERNEL_MAPPING_BASE);
	if (!table->entries[index].present) {
		return 0;
	}
	return table->entries[index].addr & ~(I686_FLAGS_MASK);
}

static void i686_VirtualMM_CreateBootstrapMapping(uint32_t cr3, uint32_t vaddr, uint32_t paddr) {
	uint16_t pdIndex = i686_VirtualMM_GetPageDirectoryIndex(vaddr);
	uint16_t ptIndex = i686_VirtualMM_GetPageTableIndex(vaddr);
	struct i686_VirtualMM_PageTable *pageDir = (struct i686_VirtualMM_PageTable *)(cr3 + I686_KERNEL_MAPPING_BASE);
	if (!(pageDir->entries[pdIndex].present)) {
		uint32_t addr = i686_PhysicalMM_KernelAllocFrame();
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

static uint16_t i686_VirtualMM_GetPageTableReferenceCount(uint32_t paddr) {
	struct i686_VirtualMM_PageTable *pageTable = (struct i686_VirtualMM_PageTable *)(paddr + I686_KERNEL_MAPPING_BASE);
	uint16_t result = 0;
	for (uint16_t i = 0; i < 1024; ++i) {
		if (pageTable->entries[i].present) {
			result++;
		}
	}
	return result;
}

void i686_VirtualMM_InitializeKernelMap() {
	uint32_t cr3 = i686_CR3_Get();
	for (uint32_t paddr = I686_KERNEL_INIT_MAPPING_SIZE; paddr < I686_PHYS_LOW_LIMIT; paddr += I686_PAGE_SIZE) {
		i686_VirtualMM_CreateBootstrapMapping(cr3, paddr + I686_KERNEL_MAPPING_BASE, paddr);
	}
	struct i686_VirtualMM_PageTable *pageDir = (struct i686_VirtualMM_PageTable *)(cr3 + I686_KERNEL_MAPPING_BASE);
	pageDir->entries[0].addr = 0;
	i686_CPU_SetCR3(i686_CPU_GetCR3());
	uint32_t refcountsSize = i686_PhysicalMM_GetMemorySize() * 2 / HAL_VirtualMM_PageSize;
	uint32_t refcounts = HAL_PhysicalMM_KernelAllocArea(refcountsSize);
	if (refcounts == 0) {
		KernelLog_ErrorMsg(I686_VIRT_MOD_NAME, "Failed to allocate frame refcount array");
	}
	m_pageRefcounts = (uint16_t *)(refcounts + HAL_VirtualMM_KernelMappingBase);
	memset(m_pageRefcounts, 0, refcountsSize);
	for (uint16_t i = 0; i < 1024; ++i) {
		if (pageDir->entries[i].present) {
			uint32_t next = i686_VirtualMM_WalkToNextPageTable(cr3, i);
			uint16_t refcount = i686_VirtualMM_GetPageTableReferenceCount(next);
			m_pageRefcounts[next / HAL_VirtualMM_PageSize] = refcount;
		}
	}
}

bool HAL_VirtualMM_MapPageAt(uintptr_t root, uintptr_t vaddr, uintptr_t paddr, int flags) {
	struct i686_VirtualMM_PageTable *pageDir = (struct i686_VirtualMM_PageTable *)(root + I686_KERNEL_MAPPING_BASE);
	uint16_t pdIndex = i686_VirtualMM_GetPageDirectoryIndex(vaddr);
	uint16_t ptIndex = i686_VirtualMM_GetPageTableIndex(vaddr);
	if (!(pageDir->entries[pdIndex].present)) {
		uint32_t addr = i686_PhysicalMM_KernelAllocFrame();
		if (addr == 0) {
			return false;
		}
		pageDir->entries[pdIndex].addr = addr;
		pageDir->entries[pdIndex].present = true;
		pageDir->entries[pdIndex].writable = true;
		pageDir->entries[pdIndex].user = true;
		m_pageRefcounts[addr / HAL_VirtualMM_PageSize] = 0;
		memset((void *)(addr + HAL_VirtualMM_KernelMappingBase), 0, 0x1000);
	}
	uint32_t next = i686_VirtualMM_WalkToNextPageTable(root, pdIndex);
	struct i686_VirtualMM_PageTable *pageTable = (struct i686_VirtualMM_PageTable *)(next + I686_KERNEL_MAPPING_BASE);
	if (pageTable->entries[ptIndex].present) {
		KernelLog_ErrorMsg(I686_VIRT_MOD_NAME, "Mapping over already mapped page is not allowed");
	}
	pageTable->entries[ptIndex].addr = paddr;
	pageTable->entries[ptIndex].present = true;
	pageTable->entries[ptIndex].writable = (flags & HAL_VIRT_FLAGS_WRITABLE) != 0;
	pageTable->entries[ptIndex].cacheDisabled = (flags & HAL_VIRT_FLAGS_DISABLE_CACHE) != 0;
	pageTable->entries[ptIndex].user = (flags & HAL_VIRT_FLAGS_USER_ACCESSIBLE) != 0;
	m_pageRefcounts[next / HAL_VirtualMM_PageSize]++;
	return true;
}

uintptr_t HAL_VirtualMM_UnmapPageAt(uintptr_t root, uintptr_t vaddr) {
	uint16_t pdIndex = i686_VirtualMM_GetPageDirectoryIndex(vaddr);
	uint16_t ptIndex = i686_VirtualMM_GetPageTableIndex(vaddr);
	uint32_t pageTablePhys = i686_VirtualMM_WalkToNextPageTable(root, pdIndex);
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
	uintptr_t result = i686_VirtualMM_WalkToNextPageTable(pageTablePhys, ptIndex);
	pageTable->entries[ptIndex].addr = 0;
	if (m_pageRefcounts[pageTablePhys / HAL_VirtualMM_PageSize] == 0) {
		KernelLog_ErrorMsg(I686_VIRT_MOD_NAME, "Attempt to decrement reference count which is already zero");
	}
	--m_pageRefcounts[pageTablePhys / HAL_VirtualMM_PageSize];
	if (m_pageRefcounts[pageTablePhys / HAL_VirtualMM_PageSize] == 0) {
		pageDir->entries[pdIndex].addr = 0;
		HAL_PhysicalMM_KernelFreeFrame(pageTablePhys);
	}
	return result;
}

void HAL_VirtualMM_ChangePagePermissions(uintptr_t root, uintptr_t vaddr, int flags) {
	uint16_t pdIndex = i686_VirtualMM_GetPageDirectoryIndex(vaddr);
	uint16_t ptIndex = i686_VirtualMM_GetPageTableIndex(vaddr);
	uint32_t pageTablePhys = i686_VirtualMM_WalkToNextPageTable(root, pdIndex);
	if (pageTablePhys == 0) {
		KernelLog_ErrorMsg(I686_VIRT_MOD_NAME, "Trying to change permissions on the page, page table for "
											   "which is not mapped");
	}
	struct i686_VirtualMM_PageTable *pageTable =
		(struct i686_VirtualMM_PageTable *)(pageTablePhys + I686_KERNEL_MAPPING_BASE);
	uintptr_t addr = i686_VirtualMM_WalkToNextPageTable(pageTablePhys, ptIndex);
	if (!(pageTable->entries[ptIndex].present)) {
		KernelLog_ErrorMsg(I686_VIRT_MOD_NAME, "Trying to unmap virtual page page which is not mapped");
	}
	pageTable->entries[ptIndex].addr = addr;
	pageTable->entries[ptIndex].writable = (flags & HAL_VIRT_FLAGS_WRITABLE) != 0;
	pageTable->entries[ptIndex].cacheDisabled = (flags & HAL_VIRT_FLAGS_DISABLE_CACHE) != 0;
	pageTable->entries[ptIndex].user = (flags & HAL_VIRT_FLAGS_USER_ACCESSIBLE) != 0;
	pageTable->entries[ptIndex].present = true;
}

uintptr_t HAL_VirtualMM_MakeNewAddressSpace() {
	uint32_t frame = i686_PhysicalMM_KernelAllocFrame();
	if (frame == 0) {
		return 0;
	}
	uint32_t *writableFrame = (uint32_t *)(frame + I686_KERNEL_MAPPING_BASE);
	uint32_t *pageDir = (uint32_t *)(i686_CR3_Get() + I686_KERNEL_MAPPING_BASE);
	memset(writableFrame, 0, I686_PAGE_SIZE);
	memcpy(writableFrame + 768, pageDir + 768, (1024 - 768) * 4);
	return frame;
}

void HAL_VirtualMM_FreeAddressSpace(uintptr_t root) {
	HAL_PhysicalMM_KernelFreeFrame(root);
}

void HAL_VirtualMM_SwitchToAddressSpace(uintptr_t root) {
	i686_CR3_Set(root);
}

uintptr_t HAL_VirtualMM_GetCurrentAddressSpace(uintptr_t root) {
	return i686_CR3_Get(root);
}

static void i686_VirtualMM_MapPageInKernelSpace(uint32_t vaddr, uint32_t paddr, bool cacheDisabled) {
	uint32_t pdIndex = i686_VirtualMM_GetPageDirectoryIndex(vaddr);
	uint32_t ptIndex = i686_VirtualMM_GetPageTableIndex(vaddr);
	uint32_t cr3 = i686_CR3_Get();
	uint32_t pageTableAddr = i686_VirtualMM_WalkToNextPageTable(cr3, pdIndex);
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
	i686_Ring0Executor_Invoke((uint32_t)i686_VirtualMM_FlushCR3FromRing0, 0);
}

uintptr_t HAL_VirtualMM_GetIOMapping(uintptr_t paddr, size_t size, bool cacheDisabled) {
	uint32_t area = HAL_PhysicalMM_KernelAllocArea(size);
	if (area == 0) {
		return 0;
	}
	for (uint32_t offset = 0; offset < size; offset += I686_PAGE_SIZE) {
		i686_VirtualMM_MapPageInKernelSpace(I686_KERNEL_MAPPING_BASE + area + offset, paddr + offset, cacheDisabled);
	}
	i686_VirtualMM_FlushCR3();
	return area + I686_KERNEL_MAPPING_BASE;
}

void HAL_VirtualMM_Flush() {
	i686_VirtualMM_FlushCR3();
}
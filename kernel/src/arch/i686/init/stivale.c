#include <arch/i686/init/stivale.h>
#include <arch/i686/memory/config.h>
#include <common/lib/kmsg.h>

#define STIVALE_MOD_NAME "i686 Stivale Parser"

struct i686_Stivale_BootInfo {
	uint64_t cmdline;
	uint64_t memoryMapAddr;
	uint64_t memoryMapEntries;
	uint64_t framebufferAddr;
	uint16_t framebufferPitch;
	uint16_t framebufferWidth;
	uint16_t framebufferHeight;
	uint16_t framebufferBPP;
	uint64_t rsdp;
	uint64_t moduleCount;
	uint64_t modules;
	uint64_t epoch;
	uint64_t flags;
};

static struct i686_Stivale_BootInfo i686_Stivale_info;

void i686_Stivale_Initialize(uint32_t phys_info) {
	if (phys_info + sizeof(struct i686_Stivale_BootInfo) > I686_KERNEL_INIT_MAPPING_SIZE) {
		KernelLog_ErrorMsg(STIVALE_MOD_NAME, "Stivale information is not visible from boot kernel mapping");
	}
	i686_Stivale_info = *(struct i686_Stivale_BootInfo *)(phys_info + I686_KERNEL_MAPPING_BASE);
}

bool i686_Stivale_GetMemoryMap(struct i686_Stivale_MemoryMap *buf) {
	if (i686_Stivale_info.memoryMapAddr + i686_Stivale_info.memoryMapEntries * sizeof(struct i686_stivale_mmap_entry) >
		(uint64_t)I686_KERNEL_INIT_MAPPING_SIZE) {
		KernelLog_WarnMsg(STIVALE_MOD_NAME, "Memory map is not visible from boot kernel mapping");
		return false;
	}
	buf->entries =
		(struct i686_stivale_mmap_entry *)((uint32_t)(i686_Stivale_info.memoryMapAddr) + I686_KERNEL_MAPPING_BASE);
	buf->entries_count = (uint32_t)(i686_Stivale_info.memoryMapEntries);
	return true;
}

bool i686_Stivale_GetFramebufferInfo(struct i686_Stivale_FramebufferInfo *buf) {
	size_t fb_size = i686_Stivale_info.framebufferHeight * i686_Stivale_info.framebufferPitch;
	if ((i686_Stivale_info.framebufferAddr) == 0 || (i686_Stivale_info.framebufferAddr + fb_size) > 0xffffffff) {
		return false;
	}
	buf->framebufferAddr = i686_Stivale_info.framebufferAddr;
	buf->framebufferPitch = i686_Stivale_info.framebufferPitch;
	buf->framebufferWidth = i686_Stivale_info.framebufferWidth;
	buf->framebufferHeight = i686_Stivale_info.framebufferHeight;
	buf->framebufferBPP = i686_Stivale_info.framebufferBPP;
	return true;
}
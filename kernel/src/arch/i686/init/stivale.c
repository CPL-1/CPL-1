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

static struct i686_Stivale_BootInfo m_StivaleInfo;

void i686_Stivale_Initialize(uint32_t phys_info) {
	if (phys_info + sizeof(struct i686_Stivale_BootInfo) > I686_KERNEL_INIT_MAPPING_SIZE) {
		KernelLog_ErrorMsg(STIVALE_MOD_NAME, "Stivale information is not visible from boot kernel mapping");
	}
	m_StivaleInfo = *(struct i686_Stivale_BootInfo *)(phys_info + I686_KERNEL_MAPPING_BASE);
}

bool i686_Stivale_GetMemoryMap(struct i686_Stivale_MemoryMap *buf) {
	if (m_StivaleInfo.memoryMapAddr + m_StivaleInfo.memoryMapEntries * sizeof(struct i686_stivale_mmap_entry) >
		(uint64_t)I686_KERNEL_INIT_MAPPING_SIZE) {
		KernelLog_WarnMsg(STIVALE_MOD_NAME, "Memory map is not visible from boot kernel mapping");
		return false;
	}
	buf->entries =
		(struct i686_stivale_mmap_entry *)((uint32_t)(m_StivaleInfo.memoryMapAddr) + I686_KERNEL_MAPPING_BASE);
	buf->entries_count = (uint32_t)(m_StivaleInfo.memoryMapEntries);
	return true;
}

bool i686_Stivale_GetFramebufferInfo(struct i686_Stivale_FramebufferInfo *buf) {
	size_t fb_size = m_StivaleInfo.framebufferHeight * m_StivaleInfo.framebufferPitch;
	if ((m_StivaleInfo.framebufferAddr) == 0 || (m_StivaleInfo.framebufferAddr + fb_size) > 0xffffffff) {
		return false;
	}
	buf->framebufferAddr = m_StivaleInfo.framebufferAddr;
	buf->framebufferPitch = m_StivaleInfo.framebufferPitch;
	buf->framebufferWidth = m_StivaleInfo.framebufferWidth;
	buf->framebufferHeight = m_StivaleInfo.framebufferHeight;
	buf->framebufferBPP = m_StivaleInfo.framebufferBPP;
	return true;
}

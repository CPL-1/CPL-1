#ifndef __STIVALE_H_INCLUDED__
#define __STIVALE_H_INCLUDED__

#include <common/misc/utils.h>

enum i686_stivale_mmap_entry_type {
	AVAILABLE = 1,
	RESERVED = 2,
	ACPI_RECLAIMABLE = 3,
	NVS = 4,
	BADRAM = 5,
	KERNEL_MODULES = 10,
	BOOTLOADER_RECLAIMABLE = 0x1000
};

struct i686_stivale_mmap_entry {
	uint64_t base;
	uint64_t length;
	uint32_t type;
	uint32_t reserved;
} PACKED;

struct i686_Stivale_FramebufferInfo {
	uint64_t framebufferAddr;
	uint16_t framebufferPitch;
	uint16_t framebufferWidth;
	uint16_t framebufferHeight;
	uint16_t framebufferBPP;
} PACKED;

struct i686_Stivale_MemoryMap {
	struct i686_stivale_mmap_entry *entries;
	uint32_t entries_count;
};

void i686_Stivale_Initialize(uint32_t phys_info);
bool i686_Stivale_GetMemoryMap(struct i686_Stivale_MemoryMap *buf);
bool i686_Stivale_GetFramebufferInfo(struct i686_Stivale_FramebufferInfo *buf);

#endif

#ifndef __STIVALE_H_INCLUDED__
#define __STIVALE_H_INCLUDED__

#include <common/misc/utils.h>

enum i686_stivale_mmap_entry_type
{
	AVAILABLE = 1,
	RESERVED = 2,
	ACPI_RECLAIMABLE = 3,
	NVS = 4,
	BADRAM = 5,
	KERNEL_MODULES = 10,
	BOOTLOADER_RECLAIMABLE = 0x1000
};

struct i686_stivale_mmap_entry {
	UINT64 base;
	UINT64 length;
	UINT32 type;
	UINT32 reserved;
} PACKED;

struct i686_Stivale_FramebufferInfo {
	UINT64 framebufferAddr;
	UINT16 framebufferPitch;
	UINT16 framebufferWidth;
	UINT16 framebufferHeight;
	UINT16 framebufferBPP;
} PACKED;

struct i686_Stivale_MemoryMap {
	struct i686_stivale_mmap_entry *entries;
	UINT32 entries_count;
};

void i686_Stivale_Initialize(UINT32 phys_info);
bool i686_Stivale_GetMemoryMap(struct i686_Stivale_MemoryMap *buf);
bool i686_Stivale_GetFramebufferInfo(struct i686_Stivale_FramebufferInfo *buf);

#endif

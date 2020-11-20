#ifndef __MULTIBOOT_H_INCLUDED__
#define __MULTIBOOT_H_INCLUDED__

#include <common/misc/utils.h>

enum i686_multiboot_mmap_entry_type {
	AVAILABLE = 1,
	RESERVED = 2,
	ACPI_RECLAIMABLE = 3,
	NVS = 4,
	BADRAM = 5
};

struct i686_multiboot_mmap_entry {
	uint32_t size;
	uint64_t addr;
	uint64_t len;
	uint32_t type;
} packed;

struct i686_multiboot_mmap {
	struct i686_multiboot_mmap_entry *entries;
	uint32_t entries_count;
};

void i686_multiboot_init(uint32_t phys_info);
bool i686_multiboot_get_mmap(struct i686_multiboot_mmap *buf);

#endif

#ifndef __MULTIBOOT_H_INCLUDED__
#define __MULTIBOOT_H_INCLUDED__

#include <utils.h>

enum multiboot_mmap_entry_type {
	AVAILABLE = 1,
	RESERVED = 2,
	ACPI_RECLAIMABLE = 3,
	NVS = 4,
	BADRAM = 5
};

struct multiboot_mmap_entry {
	uint32_t size;
	uint64_t addr;
	uint64_t len;
	uint32_t type;
} packed;

struct multiboot_mmap {
	struct multiboot_mmap_entry *entries;
	uint32_t entries_count;
};

void multiboot_init(uint32_t phys_info);
bool multiboot_get_mmap(struct multiboot_mmap *buf);

#endif

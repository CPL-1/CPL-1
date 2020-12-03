#include <arch/i686/init/stivale.h>
#include <arch/i686/memory/config.h>
#include <common/lib/kmsg.h>

#define STIVALE_MOD_NAME "i686 Stivale Parser"

struct i686_stivale_info {
	uint64_t cmdline;
	uint64_t memory_map_addr;
	uint64_t memory_map_entries;
	uint64_t framebuffer_addr;
	uint16_t framebuffer_pitch;
	uint16_t framebuffer_height;
	uint16_t framebuffer_bpp;
	uint64_t rsdp;
	uint64_t module_count;
	uint64_t modules;
	uint64_t epoch;
	uint64_t flags;
};

static struct i686_stivale_info i686_stivale_info;

void i686_stivale_init(uint32_t phys_info) {
	if (phys_info + sizeof(struct i686_stivale_info) >
		I686_KERNEL_INIT_MAPPING_SIZE) {
		kmsg_err(STIVALE_MOD_NAME,
				 "Stivale information is not visible from boot kernel mapping");
	}
	i686_stivale_info =
		*(struct i686_stivale_info *)(phys_info + I686_KERNEL_MAPPING_BASE);
}

bool i686_stivale_get_mmap(struct i686_stivale_mmap *buf) {
	printf("mmap at %p\n", i686_stivale_info.memory_map_addr);
	if (i686_stivale_info.memory_map_addr +
			i686_stivale_info.memory_map_entries *
				sizeof(struct i686_stivale_mmap_entry) >
		(uint64_t)I686_KERNEL_INIT_MAPPING_SIZE) {
		kmsg_warn(STIVALE_MOD_NAME,
				  "Memory map is not visible from boot kernel mapping");
		return false;
	}
	buf->entries = (struct i686_stivale_mmap_entry
						*)((uint32_t)(i686_stivale_info.memory_map_addr) +
						   I686_KERNEL_MAPPING_BASE);
	buf->entries_count = (uint32_t)(i686_stivale_info.memory_map_entries);
	return true;
}

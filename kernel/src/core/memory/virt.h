#ifndef __VIRT_H_INCLUDED__
#define __VIRT_H_INCLUDED__

#include <arch/i386/cpu.h>
#include <utils/utils.h>

#define FLAGS_MASK 0b111111111111

union virt_page_entry {
	uint32_t addr;
	struct {
		uint32_t present : 1;
		uint32_t writable : 1;
		uint32_t user : 1;
		uint32_t write_through : 1;
		uint32_t cache_disabled : 1;
		uint32_t accessed : 1;
		uint32_t page_size : 1;
		uint32_t ignored : 1;
		uint32_t : 24;
	} packed;
} packed;

struct virt_page_table {
	union virt_page_entry entries[1024];
} packed;

static inline uint16_t virt_pd_index(uint32_t vaddr) {
	return (vaddr >> 22) & (0b1111111111);
}

static inline uint16_t virt_pt_index(uint32_t vaddr) {
	return (vaddr >> 12) & (0b1111111111);
}

static inline uint32_t virt_walk_to_next(uint32_t current, uint16_t index) {
	struct virt_page_table *table =
		(struct virt_page_table *)(current + KERNEL_MAPPING_BASE);
	if (!table->entries[index].present) {
		return 0;
	}
	return table->entries[index].addr & ~(FLAGS_MASK);
}

void virt_kernel_mapping_init();
uint32_t virt_new_cr3();
uint32_t virt_get_io_mapping(uint32_t paddr, uint32_t size,
							 bool cache_disabled);

#endif

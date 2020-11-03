#ifndef __VIRT_H_INCLUDED__
#define __VIRT_H_INCLUDED__

#include <i386/cpu.h>
#include <utils.h>

#define FLAGS_MASK 0b111111111111

union virt_page_entry {
	uint32_t addr;
	struct {
		bool present : 1;
		bool writable : 1;
		bool user : 1;
		bool write_through : 1;
		bool cache_disabled : 1;
		bool accessed : 1;
		bool page_size : 1;
		bool ignored : 1;
		uint8_t : 3;
	} packed;
} packed;

struct virt_page_table {
	union virt_page_entry entries[1024];
} packed;

inline static uint16_t virt_pd_index(uint32_t vaddr) {
	return (vaddr >> 22) & (0b1111111111);
}

inline static uint16_t virt_pt_index(uint32_t vaddr) {
	return (vaddr >> 12) & (0b1111111111);
}

inline static uint32_t virt_walk_to_next(uint32_t current, uint16_t index) {
	struct virt_page_table *table =
	    (struct virt_page_table *)(current + KERNEL_MAPPING_BASE);
	if (!table->entries[index].present) {
		return 0;
	}
	return table->entries[index].addr & ~(FLAGS_MASK);
}

void virt_kernel_mapping_init();
uint32_t virt_new_cr3();

#endif
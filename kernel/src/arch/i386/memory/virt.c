#include <arch/i386/cpu/cr3.h>
#include <arch/i386/memory/config.h>
#include <arch/i386/memory/phys.h>
#include <arch/i386/memory/virt.h>
#include <arch/i386/proc/priv.h>
#include <hal/memory/phys.h>
#include <hal/memory/virt.h>
#include <lib/kmsg.h>

#define VIRT_MOD_NAME "i386 Virtual Memory Manager"
#define I386_FLAGS_MASK 0b111111111111

union i386_virt_page_table_entry {
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

struct i386_virt_page_table {
	union i386_virt_page_table_entry entries[1024];
} packed;

uintptr_t HAL_VIRT_KERNEL_MAPPING_BASE = I386_KERNEL_MAPPING_BASE;
size_t HAL_VIRT_PAGE_SIZE = I386_PAGE_SIZE;

static inline uint16_t i386_virt_pd_index(uint32_t vaddr) {
	return (vaddr >> 22) & (0b1111111111);
}

static inline uint16_t i386_virt_pt_index(uint32_t vaddr) {
	return (vaddr >> 12) & (0b1111111111);
}

static inline uint32_t i386_virt_walk_to_next(uint32_t current,
											  uint16_t index) {
	struct i386_virt_page_table *table =
		(struct i386_virt_page_table *)(current + I386_KERNEL_MAPPING_BASE);
	if (!table->entries[index].present) {
		return 0;
	}
	return table->entries[index].addr & ~(I386_FLAGS_MASK);
}

static void i386_virt_init_map_at(uint32_t cr3, uint32_t vaddr,
								  uint32_t paddr) {
	uint16_t pd_index = i386_virt_pd_index(vaddr);
	uint16_t pt_index = i386_virt_pt_index(vaddr);
	struct i386_virt_page_table *page_dir =
		(struct i386_virt_page_table *)(cr3 + I386_KERNEL_MAPPING_BASE);
	if (!(page_dir->entries[pd_index].present)) {
		uint32_t addr = i386_phys_krnl_alloc_frame();
		if (addr == 0) {
			kmsg_err(
				VIRT_MOD_NAME,
				"Failed to allocate page table for the kernel memory mapping");
		}
		if (addr > I386_KERNEL_INIT_MAPPING_SIZE) {
			kmsg_err(VIRT_MOD_NAME,
					 "Allocated page table is not reachable from the initial "
					 "kernel mapping");
		}
		page_dir->entries[pd_index].addr = addr;
		page_dir->entries[pd_index].present = true;
		page_dir->entries[pd_index].writable = true;
	}
	struct i386_virt_page_table *page_table =
		(struct i386_virt_page_table *)(i386_virt_walk_to_next(cr3, pd_index) +
										I386_KERNEL_MAPPING_BASE);
	page_table->entries[pt_index].addr = paddr;
	page_table->entries[pt_index].present = true;
	page_table->entries[pt_index].writable = true;
}

void i386_virt_kernel_mapping_init() {
	uint32_t cr3 = i386_cr3_get();
	for (uint32_t paddr = I386_KERNEL_INIT_MAPPING_SIZE;
		 paddr < I386_PHYS_LOW_LIMIT; paddr += I386_PAGE_SIZE) {
		i386_virt_init_map_at(cr3, paddr + I386_KERNEL_MAPPING_BASE, paddr);
	}
	struct i386_virt_page_table *page_dir =
		(struct i386_virt_page_table *)(cr3 + I386_KERNEL_MAPPING_BASE);
	page_dir->entries[0].addr = 0;
	i386_cpu_set_cr3(i386_cpu_get_cr3());
}

uintptr_t hal_virt_new_root() {
	uint32_t frame = i386_phys_krnl_alloc_frame();
	if (frame == 0) {
		return 0;
	}
	uint32_t *writable_frame = (uint32_t *)(frame + I386_KERNEL_MAPPING_BASE);
	uint32_t *page_dir =
		(uint32_t *)(i386_cr3_get() + I386_KERNEL_MAPPING_BASE);
	memset(writable_frame, 0, I386_PAGE_SIZE);
	memcpy(writable_frame + 768, page_dir + 768, (1024 - 768) * 4);
	return frame;
}

void hal_virt_free_root(uintptr_t root) { i386_phys_krnl_free_frame(root); }

void hal_virt_set_root(uintptr_t root) { i386_cr3_set(root); }

uintptr_t hal_virt_get_root(uintptr_t root) { return i386_cr3_get(root); }

static void i386_virt_map_in_kernel_part(uint32_t vaddr, uint32_t paddr,
										 bool cache_disabled) {
	uint32_t pd_index = i386_virt_pd_index(vaddr);
	uint32_t pt_index = i386_virt_pt_index(vaddr);
	uint32_t cr3 = i386_cr3_get();
	uint32_t page_table_addr = i386_virt_walk_to_next(cr3, pd_index);
	struct i386_virt_page_table *table =
		(struct i386_virt_page_table *)(page_table_addr +
										I386_KERNEL_MAPPING_BASE);
	table->entries[pt_index].addr = paddr;
	table->entries[pt_index].present = true;
	table->entries[pt_index].writable = true;
	table->entries[pt_index].write_through = true;
	table->entries[pt_index].cache_disabled = cache_disabled;
}

static void i386_virt_flush_i386_cr3_ring0() { i386_cr3_set(i386_cr3_get()); }

static void i386_virt_flush_cr3() {
	priv_call_ring0((int)i386_virt_flush_i386_cr3_ring0, 0);
}

uintptr_t hal_virt_get_io_mapping(uintptr_t paddr, size_t size,
								  bool cache_disabled) {
	uint32_t area = hal_phys_krnl_alloc_area(size);
	if (area == 0) {
		return 0;
	}
	for (uint32_t offset = 0; offset < size; offset += I386_PAGE_SIZE) {
		i386_virt_map_in_kernel_part(I386_KERNEL_MAPPING_BASE + area + offset,
									 paddr + offset, cache_disabled);
	}
	i386_virt_flush_cr3();
	return area + I386_KERNEL_MAPPING_BASE;
}
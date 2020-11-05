#include <i386/cr3.h>
#include <kmsg.h>
#include <memory/phys.h>
#include <memory/virt.h>

#define VIRT_MOD_NAME "Kernel virtual memory mapper"

static void virt_init_map_at(uint32_t cr3, uint32_t vaddr, uint32_t paddr) {
	uint16_t pd_index = virt_pd_index(vaddr);
	uint16_t pt_index = virt_pt_index(vaddr);
	struct virt_page_table *page_dir =
	    (struct virt_page_table *)(cr3 + KERNEL_MAPPING_BASE);
	if (!(page_dir->entries[pd_index].present)) {
		uint32_t addr = phys_lo_alloc_frame();
		if (addr == 0) {
			kmsg_err(
			    VIRT_MOD_NAME,
			    "Failed to allocate page table for the kernel memory mapping");
		}
		if (addr > KERNEL_INIT_MAPPING_SIZE) {
			kmsg_err(VIRT_MOD_NAME,
			         "Allocated page table is not reachable from the initial "
			         "kernel mapping");
		}
		page_dir->entries[pd_index].addr = addr;
		page_dir->entries[pd_index].present = true;
		page_dir->entries[pd_index].writable = true;
	}
	struct virt_page_table *page_table =
	    (struct virt_page_table *)(virt_walk_to_next(cr3, pd_index) +
	                               KERNEL_MAPPING_BASE);
	page_table->entries[pt_index].addr = paddr;
	page_table->entries[pt_index].present = true;
	page_table->entries[pt_index].writable = true;
	cpu_invlpg(vaddr);
}

void virt_kernel_mapping_init() {
	uint32_t cr3 = cr3_get();
	for (uint32_t paddr = KERNEL_INIT_MAPPING_SIZE; paddr < PHYS_LOW_LIMIT;
	     paddr += PAGE_SIZE) {
		virt_init_map_at(cr3, paddr + KERNEL_MAPPING_BASE, paddr);
	}
	struct virt_page_table *page_dir =
	    (struct virt_page_table *)(cr3 + KERNEL_MAPPING_BASE);
	page_dir->entries[0].addr = 0;
	cpu_invlpg(0);
}

uint32_t virt_new_cr3() {
	uint32_t frame = phys_lo_alloc_frame();
	if (frame == 0) {
		return 0;
	}
	uint32_t *writable_frame = (uint32_t *)(frame + KERNEL_MAPPING_BASE);
	uint32_t *page_dir = (uint32_t *)(cr3_get() + KERNEL_MAPPING_BASE);
	memset(writable_frame, 0, PAGE_SIZE);
	memcpy(writable_frame + 768, page_dir + 768, (1024 - 768) * 4);
	return frame;
}

#include <boot/multiboot.h>
#include <config.h>
#include <kmsg.h>

#define MULTIBOOT_MOD_NAME "Multiboot Parser"

struct multiboot_info {
	uint32_t flags;

	uint32_t mem_lower;
	uint32_t mem_upper;

	uint32_t boot_dev;

	uint32_t cmdline;

	uint32_t mods_count;
	uint32_t mods_addr;

	struct multiboot_elf_section_header_table {
		uint32_t tabsize;
		uint32_t strsize;
		uint32_t addr;
		uint32_t reserved;
	} packed elf_sec;

	uint32_t mmap_length;
	uint32_t mmap_addr;

	uint32_t drives_length;
	uint32_t drives_addr;

	uint32_t config_table;

	uint32_t bootloader_name;

	uint32_t apm_table;

	uint32_t vbe_control_info;
	uint32_t vbe_mode_info;
	uint32_t vbe_mode;
	uint32_t vbe_interface_seg;
	uint32_t vbe_interface_off;
	uint32_t vbe_interface_len;

	uint64_t framebuffer_addr;
	uint32_t framebuffer_pitch;
	uint32_t framebuffer_width;
	uint32_t framebuffer_height;
	uint8_t framebuffer_bpp;
	uint8_t framebuffer_type;

	union {
		struct {
			uint32_t framebuffer_palette_addr;
			uint32_t framebuffer_palette_num_colors;
		} packed;
		struct {
			uint8_t red_field_position;
			uint8_t red_mask_size;
			uint8_t green_field_position;
			uint8_t green_mask_size;
			uint8_t blue_field_position;
			uint8_t blue_mask_size;
		} packed;
	} packed;
} packed;

static struct multiboot_info *mb_info;

void multiboot_init(uint32_t phys_info) {
	mb_info = (struct multiboot_info *)(phys_info + KERNEL_MAPPING_BASE);
	if (phys_info + sizeof(struct multiboot_info) > KERNEL_INIT_MAPPING_SIZE) {
		kmsg_err(
			MULTIBOOT_MOD_NAME,
			"Multiboot information is not visible from boot kernel mapping");
	}
}

bool multiboot_get_mmap(struct multiboot_mmap *buf) {
	if ((mb_info->flags & (1 << 6)) == 0) {
		return false;
	}
	if (mb_info->mmap_addr + mb_info->mmap_length > KERNEL_INIT_MAPPING_SIZE) {
		kmsg_warn(MULTIBOOT_MOD_NAME,
				  "Memory map is not visible from boot kernel mapping");
		return false;
	}
	buf->entries = (struct multiboot_mmap_entry *)(mb_info->mmap_addr +
												   KERNEL_MAPPING_BASE);
	buf->entries_count = mb_info->mmap_length;
	return true;
}

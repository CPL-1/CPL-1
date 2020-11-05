#include <boot/multiboot.h>
#include <kmsg.h>
#include <memory/phys.h>

#define PHYS_MOD_NAME "phys"

extern int phys_kernel_end;

static uint32_t phys_bitmap[0x5000];

static uint32_t phys_low_arena_lo = KERNEL_IGNORED_AREA_SIZE / PAGE_SIZE;
static const uint32_t phys_low_arena_hi = PHYS_LOW_LIMIT / PAGE_SIZE;
static uint32_t phys_high_arena_lo = phys_low_arena_hi;
static const uint32_t phys_high_arena_hi = 0x5000;

static void phys_bit_set(uint32_t index) {
	phys_bitmap[index / 32] |= (1 << (index % 32));
}

static void phys_bit_clear(uint32_t index) {
	phys_bitmap[index / 32] &= ~(1 << (index % 32));
}

static void phys_bitmap_clear_range(uint32_t start, uint32_t size) {
	uint32_t end = start + size;
	for (uint32_t i = (start / PAGE_SIZE); i < (end / PAGE_SIZE); ++i) {
		phys_bit_clear(i);
	}
}

static void phys_bitmap_set_range(uint32_t start, uint32_t size) {
	uint32_t end = start + size;
	for (uint32_t i = (start / PAGE_SIZE); i < (end / PAGE_SIZE); ++i) {
		phys_bit_set(i);
	}
}

static bool phys_bit_get(uint32_t index) {
	return (phys_bitmap[index / 32] & (1 << (index % 32))) != 0;
}

uint32_t phys_lo_alloc_frame() {
	for (; phys_low_arena_lo < phys_low_arena_hi; ++phys_low_arena_lo) {
		if (!phys_bit_get(phys_low_arena_lo)) {
			phys_bit_set(phys_low_arena_lo);
			return phys_low_arena_lo * PAGE_SIZE;
		}
	}
	return 0;
}

uint32_t phys_hi_alloc_frame() {
	for (; phys_high_arena_lo < phys_high_arena_hi; ++phys_high_arena_lo) {
		if (!phys_bit_get(phys_high_arena_lo)) {
			phys_bit_set(phys_high_arena_lo);
			return phys_high_arena_lo * PAGE_SIZE;
		}
	}
	return 0;
}

uint32_t phys_lo_alloc_area(uint32_t size) {
	const uint32_t frames_needed = size / PAGE_SIZE;
	uint32_t free_frames = 0;
	uint32_t result_ind = phys_low_arena_lo;
	uint32_t current_ind = phys_low_arena_lo;
	while (current_ind < phys_low_arena_hi) {
		if (phys_bit_get(current_ind)) {
			free_frames = 0;
			result_ind = current_ind + 1;
		} else {
			++free_frames;
		}
		if (free_frames >= frames_needed) {
			phys_bitmap_set_range(result_ind * PAGE_SIZE, size);
			if (result_ind == phys_low_arena_lo) {
				phys_low_arena_lo += frames_needed;
			}
			return result_ind * PAGE_SIZE;
		}
		++current_ind;
	}
	return 0;
}

void phys_free_frame(uint32_t frame) {
	uint32_t index = frame / PAGE_SIZE;
	if (index < phys_low_arena_hi) {
		if (index < phys_low_arena_lo) {
			phys_low_arena_lo = index;
		}
	} else {
		if (index < phys_high_arena_lo) {
			phys_high_arena_lo = index;
		}
	}
	phys_bit_clear(frame / PAGE_SIZE);
}

void phys_lo_free_area(uint32_t start, uint32_t size) {
	for (uint32_t offset = 0; offset < size; offset += PAGE_SIZE) {
		phys_free_frame(start + offset);
	}
}

void phys_init() {
	memset(&phys_bitmap, 0xff, sizeof(phys_bitmap));
	struct multiboot_mmap mmap_buf;
	if (!multiboot_get_mmap(&mmap_buf)) {
		kmsg_err(PHYS_MOD_NAME, "No memory map present");
	}
	for (uint32_t i = 0; i < mmap_buf.entries_count; ++i) {
		uint32_t entry_type = mmap_buf.entries[i].type;
		if (entry_type == AVAILABLE) {
			if (mmap_buf.entries[i].len + mmap_buf.entries[i].addr >
			        0x100000000ULL ||
			    mmap_buf.entries[i].len > 0xffffffffULL ||
			    mmap_buf.entries[i].addr > 0xffffffffULL) {
				kmsg_warn(PHYS_MOD_NAME,
				          "area that is not visible in protected mode was "
				          "found. This area will be ignored\n");
				continue;
			}
			uint32_t len = (uint32_t)(mmap_buf.entries[i].len);
			uint32_t addr = (uint32_t)(mmap_buf.entries[i].addr);
			phys_bitmap_clear_range(addr, len);
		}
	}
	phys_bitmap_set_range(0, ALIGN_UP((uint32_t)&phys_kernel_end, PAGE_SIZE));
}

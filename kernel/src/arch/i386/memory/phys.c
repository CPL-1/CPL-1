#include <arch/i386/init/multiboot.h>
#include <arch/i386/memory/config.h>
#include <arch/i386/memory/phys.h>
#include <arch/memory/phys.h>
#include <core/proc/mutex.h>
#include <lib/kmsg.h>

#define PHYS_MOD_NAME "i386 Physical Memory Manager"

extern uint32_t i386_phys_kernel_end;

static uint32_t i386_phys_bitmap[0x5000];

static uint32_t i386_phys_low_arena_lo =
	I386_KERNEL_IGNORED_AREA_SIZE / I386_PAGE_SIZE;
static const uint32_t i386_phys_low_arena_hi =
	I386_PHYS_LOW_LIMIT / I386_PAGE_SIZE;
static uint32_t i386_phys_high_arena_lo = i386_phys_low_arena_hi;
static const uint32_t i386_phys_high_arena_hi = 0x5000;
static struct mutex i386_phys_mutex;

static void i386_phys_bit_set(uint32_t index) {
	i386_phys_bitmap[index / 32] |= (1 << (index % 32));
}

static void i386_phys_bit_clear(uint32_t index) {
	i386_phys_bitmap[index / 32] &= ~(1 << (index % 32));
}

static void i386_phys_bitmap_clear_range(uint32_t start, uint32_t size) {
	uint32_t end = start + size;
	for (uint32_t i = (start / I386_PAGE_SIZE); i < (end / I386_PAGE_SIZE);
		 ++i) {
		i386_phys_bit_clear(i);
	}
}

static void i386_phys_bitmap_set_range(uint32_t start, uint32_t size) {
	uint32_t end = start + size;
	for (uint32_t i = (start / I386_PAGE_SIZE); i < (end / I386_PAGE_SIZE);
		 ++i) {
		i386_phys_bit_set(i);
	}
}

static bool i386_phys_bit_get(uint32_t index) {
	return (i386_phys_bitmap[index / 32] & (1 << (index % 32))) != 0;
}

uintptr_t i386_phys_krnl_alloc_frame() {
	mutex_lock(&i386_phys_mutex);
	for (; i386_phys_low_arena_lo < i386_phys_low_arena_hi;
		 ++i386_phys_low_arena_lo) {
		if (!i386_phys_bit_get(i386_phys_low_arena_lo)) {
			i386_phys_bit_set(i386_phys_low_arena_lo);
			mutex_unlock(&i386_phys_mutex);
			return i386_phys_low_arena_lo * I386_PAGE_SIZE;
		}
	}
	mutex_unlock(&i386_phys_mutex);
	return 0;
}

uintptr_t arch_phys_user_alloc_frame() {
	mutex_lock(&i386_phys_mutex);
	for (; i386_phys_high_arena_lo < i386_phys_high_arena_hi;
		 ++i386_phys_high_arena_lo) {
		if (!i386_phys_bit_get(i386_phys_high_arena_lo)) {
			i386_phys_bit_set(i386_phys_high_arena_lo);
			mutex_unlock(&i386_phys_mutex);
			return i386_phys_high_arena_lo * I386_PAGE_SIZE;
		}
	}
	mutex_unlock(&i386_phys_mutex);
	return 0;
}

uintptr_t arch_phys_krnl_alloc_area(size_t size) {
	mutex_lock(&i386_phys_mutex);
	const uint32_t frames_needed = size / I386_PAGE_SIZE;
	uint32_t free_frames = 0;
	uint32_t result_ind = i386_phys_low_arena_lo;
	uint32_t current_ind = i386_phys_low_arena_lo;
	while (current_ind < i386_phys_low_arena_hi) {
		if (i386_phys_bit_get(current_ind)) {
			free_frames = 0;
			result_ind = current_ind + 1;
		} else {
			++free_frames;
		}
		if (free_frames >= frames_needed) {
			i386_phys_bitmap_set_range(result_ind * I386_PAGE_SIZE, size);
			if (result_ind == i386_phys_low_arena_lo) {
				i386_phys_low_arena_lo += frames_needed;
			}
			mutex_unlock(&i386_phys_mutex);
			return result_ind * I386_PAGE_SIZE;
		}
		++current_ind;
	}
	mutex_unlock(&i386_phys_mutex);
	return 0;
}

static void i386_phys_free_frame(uint32_t frame) {
	mutex_lock(&i386_phys_mutex);
	uint32_t index = frame / I386_PAGE_SIZE;
	if (index < i386_phys_low_arena_hi) {
		if (index < i386_phys_low_arena_lo) {
			i386_phys_low_arena_lo = index;
		}
	} else {
		if (index < i386_phys_high_arena_lo) {
			i386_phys_high_arena_lo = index;
		}
	}
	i386_phys_bit_clear(frame / I386_PAGE_SIZE);
	mutex_unlock(&i386_phys_mutex);
}

void arch_phys_user_free_frame(uintptr_t frame) { i386_phys_free_frame(frame); }
void i386_phys_krnl_free_frame(uint32_t frame) { i386_phys_free_frame(frame); }

void arch_phys_krnl_free_area(uintptr_t area, size_t size) {
	size = ALIGN_UP(size, I386_PAGE_SIZE);
	mutex_lock(&i386_phys_mutex);
	for (uint32_t offset = 0; offset < size; offset += I386_PAGE_SIZE) {
		i386_phys_free_frame(area + offset);
	}
	mutex_unlock(&i386_phys_mutex);
}

void i386_phys_init() {
	mutex_init(&i386_phys_mutex);
	memset(&i386_phys_bitmap, 0xff, sizeof(i386_phys_bitmap));
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
			i386_phys_bitmap_clear_range(addr, len);
		}
	}
	i386_phys_bitmap_set_range(
		0, ALIGN_UP((uint32_t)&i386_phys_kernel_end, I386_PAGE_SIZE));
}

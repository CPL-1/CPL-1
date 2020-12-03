#include <arch/i686/init/stivale.h>
#include <arch/i686/memory/config.h>
#include <arch/i686/memory/phys.h>
#include <common/core/proc/mutex.h>
#include <common/lib/kmsg.h>
#include <hal/memory/phys.h>

#define PHYS_MOD_NAME "i686 Physical Memory Manager"

extern uint32_t i686_phys_kernel_end;

static uint32_t i686_phys_bitmap[0x5000];

static uint32_t i686_phys_low_arena_lo =
	I686_KERNEL_IGNORED_AREA_SIZE / I686_PAGE_SIZE;
static const uint32_t i686_phys_low_arena_hi =
	I686_PHYS_LOW_LIMIT / I686_PAGE_SIZE;
static uint32_t i686_phys_high_arena_lo = i686_phys_low_arena_hi;
static const uint32_t i686_phys_high_arena_hi = 0xa0000;
static struct mutex i686_phys_mutex;
uint32_t i686_phys_limit;

static void i686_phys_bit_set(uint32_t index) {
	i686_phys_bitmap[index / 32] |= (1 << (index % 32));
}

static void i686_phys_bit_clear(uint32_t index) {
	i686_phys_bitmap[index / 32] &= ~(1 << (index % 32));
}

static void i686_phys_bitmap_clear_range(uint32_t start, uint32_t size) {
	uint32_t end = start + size;
	for (uint32_t i = (start / I686_PAGE_SIZE); i < (end / I686_PAGE_SIZE);
		 ++i) {
		i686_phys_bit_clear(i);
	}
}

static void i686_phys_bitmap_set_range(uint32_t start, uint32_t size) {
	uint32_t end = start + size;
	for (uint32_t i = (start / I686_PAGE_SIZE); i < (end / I686_PAGE_SIZE);
		 ++i) {
		i686_phys_bit_set(i);
	}
}

static bool i686_phys_bit_get(uint32_t index) {
	return (i686_phys_bitmap[index / 32] & (1 << (index % 32))) != 0;
}

uint32_t i686_phys_krnl_alloc_frame() {
	mutex_lock(&i686_phys_mutex);
	for (; i686_phys_low_arena_lo < i686_phys_low_arena_hi;
		 ++i686_phys_low_arena_lo) {
		if (!i686_phys_bit_get(i686_phys_low_arena_lo)) {
			i686_phys_bit_set(i686_phys_low_arena_lo);
			mutex_unlock(&i686_phys_mutex);
			return i686_phys_low_arena_lo * I686_PAGE_SIZE;
		}
	}
	mutex_unlock(&i686_phys_mutex);
	return 0;
}

uintptr_t hal_phys_user_alloc_frame() {
	mutex_lock(&i686_phys_mutex);
	for (; i686_phys_high_arena_lo < i686_phys_high_arena_hi;
		 ++i686_phys_high_arena_lo) {
		if (!i686_phys_bit_get(i686_phys_high_arena_lo)) {
			i686_phys_bit_set(i686_phys_high_arena_lo);
			mutex_unlock(&i686_phys_mutex);
			return i686_phys_high_arena_lo * I686_PAGE_SIZE;
		}
	}
	mutex_unlock(&i686_phys_mutex);
	return i686_phys_krnl_alloc_frame();
}

uintptr_t hal_phys_krnl_alloc_area(size_t size) {
	mutex_lock(&i686_phys_mutex);
	const uint32_t frames_needed = size / I686_PAGE_SIZE;
	uint32_t free_frames = 0;
	uint32_t result_ind = i686_phys_low_arena_lo;
	uint32_t current_ind = i686_phys_low_arena_lo;
	while (current_ind < i686_phys_low_arena_hi) {
		if (i686_phys_bit_get(current_ind)) {
			free_frames = 0;
			result_ind = current_ind + 1;
		} else {
			++free_frames;
		}
		if (free_frames >= frames_needed) {
			i686_phys_bitmap_set_range(result_ind * I686_PAGE_SIZE, size);
			if (result_ind == i686_phys_low_arena_lo) {
				i686_phys_low_arena_lo += frames_needed;
			}
			mutex_unlock(&i686_phys_mutex);
			return result_ind * I686_PAGE_SIZE;
		}
		++current_ind;
	}
	mutex_unlock(&i686_phys_mutex);
	return 0;
}

static void i686_phys_free_frame(uint32_t frame) {
	mutex_lock(&i686_phys_mutex);
	uint32_t index = frame / I686_PAGE_SIZE;
	if (index < i686_phys_low_arena_hi) {
		if (index < i686_phys_low_arena_lo) {
			i686_phys_low_arena_lo = index;
		}
	} else {
		if (index < i686_phys_high_arena_lo) {
			i686_phys_high_arena_lo = index;
		}
	}
	i686_phys_bit_clear(frame / I686_PAGE_SIZE);
	mutex_unlock(&i686_phys_mutex);
}

void hal_phys_user_free_frame(uintptr_t frame) { i686_phys_free_frame(frame); }
void i686_phys_krnl_free_frame(uint32_t frame) { i686_phys_free_frame(frame); }

void hal_phys_krnl_free_area(uintptr_t area, size_t size) {
	size = ALIGN_UP(size, I686_PAGE_SIZE);
	mutex_lock(&i686_phys_mutex);
	for (uint32_t offset = 0; offset < size; offset += I686_PAGE_SIZE) {
		i686_phys_free_frame(area + offset);
	}
	mutex_unlock(&i686_phys_mutex);
}

void i686_phys_init() {
	mutex_init(&i686_phys_mutex);
	memset(&i686_phys_bitmap, 0xff, sizeof(i686_phys_bitmap));
	struct i686_stivale_mmap mmap_buf;
	if (!i686_stivale_get_mmap(&mmap_buf)) {
		kmsg_err(PHYS_MOD_NAME, "No memory map present");
	}
	uint32_t max = 0;
	for (uint32_t i = 0; i < mmap_buf.entries_count; ++i) {
		uint32_t entry_type = mmap_buf.entries[i].type;
		if (entry_type == AVAILABLE) {
			if (mmap_buf.entries[i].length + mmap_buf.entries[i].base >
					0x100000000ULL ||
				mmap_buf.entries[i].length > 0xffffffffULL ||
				mmap_buf.entries[i].base > 0xffffffffULL) {
				kmsg_warn(PHYS_MOD_NAME,
						  "area that is not visible in protected mode was "
						  "found. This area will be ignored\n");
				continue;
			}
			uint32_t len = (uint32_t)(mmap_buf.entries[i].length);
			uint32_t addr = (uint32_t)(mmap_buf.entries[i].base);
			if (len + addr > max) {
				max = len + addr;
			}
			i686_phys_bitmap_clear_range(addr, len);
		}
	}
	i686_phys_bitmap_set_range(
		0, ALIGN_UP((uint32_t)&i686_phys_kernel_end, I686_PAGE_SIZE));
	i686_phys_limit = max;
}

uint32_t i686_phys_get_mem_size() { return i686_phys_limit; }
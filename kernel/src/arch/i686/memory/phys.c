#include <arch/i686/init/stivale.h>
#include <arch/i686/memory/config.h>
#include <arch/i686/memory/phys.h>
#include <common/core/proc/mutex.h>
#include <common/lib/kmsg.h>
#include <hal/memory/phys.h>

#define PHYS_MOD_NAME "i686 Physical Memory Manager"

extern UINT32 i686_PhysicalMM_KernelEnd;
static UINT32 i686_PhysicalMM_Bitmap[0x5000];
static UINT32 i686_PhysicalMM_LowArenaMinIndex = I686_KERNEL_IGNORED_AREA_SIZE / I686_PAGE_SIZE;
static const UINT32 i686_PhysicalMM_LowArenaMaxIndex = I686_PHYS_LOW_LIMIT / I686_PAGE_SIZE;
static UINT32 i686_PhysicalMM_HighArenaMinIndex = i686_PhysicalMM_LowArenaMaxIndex;
static const UINT32 i686_PhysicalMM_HighArenaMaxIndex = 0xa0000;
static struct Mutex i686_PhysicalMM_Mutex;
UINT32 i686_PhysicalMM_Limit;

static void i686_PhysicalMM_SetBit(UINT32 index) {
	i686_PhysicalMM_Bitmap[index / 32] |= (1 << (index % 32));
}

static void i686_PhysicalMM_ClearBit(UINT32 index) {
	i686_PhysicalMM_Bitmap[index / 32] &= ~(1 << (index % 32));
}

static void i686_PhysicalMM_ClearRange(UINT32 start, UINT32 size) {
	UINT32 end = start + size;
	for (UINT32 i = (start / I686_PAGE_SIZE); i < (end / I686_PAGE_SIZE); ++i) {
		i686_PhysicalMM_ClearBit(i);
	}
}

static void i686_PhysicalMM_SetRange(UINT32 start, UINT32 size) {
	UINT32 end = start + size;
	for (UINT32 i = (start / I686_PAGE_SIZE); i < (end / I686_PAGE_SIZE); ++i) {
		i686_PhysicalMM_SetBit(i);
	}
}

static bool i686_PhysicalMM_GetBit(UINT32 index) {
	return (i686_PhysicalMM_Bitmap[index / 32] & (1 << (index % 32))) != 0;
}

UINT32 i686_PhysicalMM_KernelAllocFrame() {
	Mutex_Lock(&i686_PhysicalMM_Mutex);
	for (; i686_PhysicalMM_LowArenaMinIndex < i686_PhysicalMM_LowArenaMaxIndex; ++i686_PhysicalMM_LowArenaMinIndex) {
		if (!i686_PhysicalMM_GetBit(i686_PhysicalMM_LowArenaMinIndex)) {
			i686_PhysicalMM_SetBit(i686_PhysicalMM_LowArenaMinIndex);
			Mutex_Unlock(&i686_PhysicalMM_Mutex);
			return i686_PhysicalMM_LowArenaMinIndex * I686_PAGE_SIZE;
		}
	}
	Mutex_Unlock(&i686_PhysicalMM_Mutex);
	return 0;
}

UINTN HAL_PhysicalMM_UserAllocFrame() {
	Mutex_Lock(&i686_PhysicalMM_Mutex);
	for (; i686_PhysicalMM_HighArenaMinIndex < i686_PhysicalMM_HighArenaMaxIndex; ++i686_PhysicalMM_HighArenaMinIndex) {
		if (!i686_PhysicalMM_GetBit(i686_PhysicalMM_HighArenaMinIndex)) {
			i686_PhysicalMM_SetBit(i686_PhysicalMM_HighArenaMinIndex);
			Mutex_Unlock(&i686_PhysicalMM_Mutex);
			return i686_PhysicalMM_HighArenaMinIndex * I686_PAGE_SIZE;
		}
	}
	Mutex_Unlock(&i686_PhysicalMM_Mutex);
	return i686_PhysicalMM_KernelAllocFrame();
}

UINTN HAL_PhysicalMM_KernelAllocArea(USIZE size) {
	Mutex_Lock(&i686_PhysicalMM_Mutex);
	const UINT32 frames_needed = size / I686_PAGE_SIZE;
	UINT32 freeFrames = 0;
	UINT32 resultIndex = i686_PhysicalMM_LowArenaMinIndex;
	UINT32 currentIndex = i686_PhysicalMM_LowArenaMinIndex;
	while (currentIndex < i686_PhysicalMM_LowArenaMaxIndex) {
		if (i686_PhysicalMM_GetBit(currentIndex)) {
			freeFrames = 0;
			resultIndex = currentIndex + 1;
		} else {
			++freeFrames;
		}
		if (freeFrames >= frames_needed) {
			i686_PhysicalMM_SetRange(resultIndex * I686_PAGE_SIZE, size);
			if (resultIndex == i686_PhysicalMM_LowArenaMinIndex) {
				i686_PhysicalMM_LowArenaMinIndex += frames_needed;
			}
			Mutex_Unlock(&i686_PhysicalMM_Mutex);
			return resultIndex * I686_PAGE_SIZE;
		}
		++currentIndex;
	}
	Mutex_Unlock(&i686_PhysicalMM_Mutex);
	return 0;
}

static void i686_PhysicalMM_FreeFrame(UINT32 frame) {
	Mutex_Lock(&i686_PhysicalMM_Mutex);
	UINT32 index = frame / I686_PAGE_SIZE;
	if (index < i686_PhysicalMM_LowArenaMaxIndex) {
		if (index < i686_PhysicalMM_LowArenaMinIndex) {
			i686_PhysicalMM_LowArenaMinIndex = index;
		}
	} else {
		if (index < i686_PhysicalMM_HighArenaMinIndex) {
			i686_PhysicalMM_HighArenaMinIndex = index;
		}
	}
	i686_PhysicalMM_ClearBit(frame / I686_PAGE_SIZE);
	Mutex_Unlock(&i686_PhysicalMM_Mutex);
}

void HAL_PhysicalMM_UserFreeFrame(UINTN frame) {
	i686_PhysicalMM_FreeFrame(frame);
}
void HAL_PhysicalMM_KernelFreeFrame(UINT32 frame) {
	i686_PhysicalMM_FreeFrame(frame);
}

void HAL_PhysicalMM_KernelFreeArea(UINTN area, USIZE size) {
	size = ALIGN_UP(size, I686_PAGE_SIZE);
	Mutex_Lock(&i686_PhysicalMM_Mutex);
	for (UINT32 offset = 0; offset < size; offset += I686_PAGE_SIZE) {
		i686_PhysicalMM_FreeFrame(area + offset);
	}
	Mutex_Unlock(&i686_PhysicalMM_Mutex);
}

void i686_PhysicalMM_Initialize() {
	Mutex_Initialize(&i686_PhysicalMM_Mutex);
	memset(&i686_PhysicalMM_Bitmap, 0xff, sizeof(i686_PhysicalMM_Bitmap));
	struct i686_Stivale_MemoryMap mmap_buf;
	if (!i686_Stivale_GetMemoryMap(&mmap_buf)) {
		KernelLog_ErrorMsg(PHYS_MOD_NAME, "No memory map present");
	}
	UINT32 max = 0;
	for (UINT32 i = 0; i < mmap_buf.entries_count; ++i) {
		UINT32 entry_type = mmap_buf.entries[i].type;
		if (entry_type == AVAILABLE) {
			if (mmap_buf.entries[i].length + mmap_buf.entries[i].base > 0x100000000ULL ||
				mmap_buf.entries[i].length > 0xffffffffULL || mmap_buf.entries[i].base > 0xffffffffULL) {
				KernelLog_WarnMsg(PHYS_MOD_NAME, "area that is not visible in protected mode was "
												 "found. This area will be ignored\n");
				continue;
			}
			UINT32 len = (UINT32)(mmap_buf.entries[i].length);
			UINT32 addr = (UINT32)(mmap_buf.entries[i].base);
			if (len + addr > max) {
				max = len + addr;
			}
			i686_PhysicalMM_ClearRange(addr, len);
		}
	}
	i686_PhysicalMM_SetRange(0, ALIGN_UP((UINT32)&i686_PhysicalMM_KernelEnd, I686_PAGE_SIZE));
	i686_PhysicalMM_Limit = max;
}

UINT32 i686_PhysicalMM_GetMemorySize() {
	return i686_PhysicalMM_Limit;
}
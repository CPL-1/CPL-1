#include <arch/i686/init/stivale.h>
#include <arch/i686/memory/config.h>
#include <arch/i686/memory/phys.h>
#include <common/core/proc/mutex.h>
#include <common/lib/kmsg.h>
#include <hal/memory/phys.h>

#define PHYS_MOD_NAME "i686 Physical Memory Manager"

extern uint32_t i686_PhysicalMM_KernelEnd;
static uint32_t m_bitmap[0x5000];
static uint32_t m_lowArenaMinIndex = I686_KERNEL_IGNORED_AREA_SIZE / I686_PAGE_SIZE;
static uint32_t m_lowArenaMaxIndex = I686_PHYS_LOW_LIMIT / I686_PAGE_SIZE;
static uint32_t m_highArenaMinIndex = I686_PHYS_LOW_LIMIT / I686_PAGE_SIZE;
static uint32_t m_highArenaMaxIndex = 0xa0000;
static struct Mutex m_mutex;
uint32_t m_memoryLimit;

static void i686_PhysicalMM_SetBit(uint32_t index) {
	m_bitmap[index / 32] |= (1U << (index % 32));
}

static void i686_PhysicalMM_ClearBit(uint32_t index) {
	m_bitmap[index / 32] &= ~(1U << (index % 32));
}

static void i686_PhysicalMM_ClearRange(uint32_t start, uint32_t size) {
	uint32_t end = start + size;
	for (uint32_t i = (start / I686_PAGE_SIZE); i < (end / I686_PAGE_SIZE); ++i) {
		i686_PhysicalMM_ClearBit(i);
	}
}

static void i686_PhysicalMM_SetRange(uint32_t start, uint32_t size) {
	uint32_t end = start + size;
	for (uint32_t i = (start / I686_PAGE_SIZE); i < (end / I686_PAGE_SIZE); ++i) {
		i686_PhysicalMM_SetBit(i);
	}
}

static bool i686_PhysicalMM_GetBit(uint32_t index) {
	return (m_bitmap[index / 32] & (1U << (index % 32))) != 0;
}

uint32_t i686_PhysicalMM_KernelAllocFrame() {
	Mutex_Lock(&m_mutex);
	for (; m_lowArenaMinIndex < m_lowArenaMaxIndex; ++m_lowArenaMinIndex) {
		if (!i686_PhysicalMM_GetBit(m_lowArenaMinIndex)) {
			i686_PhysicalMM_SetBit(m_lowArenaMinIndex);
			Mutex_Unlock(&m_mutex);
			return m_lowArenaMinIndex * I686_PAGE_SIZE;
		}
	}
	Mutex_Unlock(&m_mutex);
	return 0;
}

uintptr_t HAL_PhysicalMM_UserAllocFrame() {
	Mutex_Lock(&m_mutex);
	for (; m_highArenaMinIndex < m_highArenaMaxIndex; ++m_highArenaMinIndex) {
		if (!i686_PhysicalMM_GetBit(m_highArenaMinIndex)) {
			i686_PhysicalMM_SetBit(m_highArenaMinIndex);
			Mutex_Unlock(&m_mutex);
			return m_highArenaMinIndex * I686_PAGE_SIZE;
		}
	}
	Mutex_Unlock(&m_mutex);
	return i686_PhysicalMM_KernelAllocFrame();
}

uintptr_t HAL_PhysicalMM_KernelAllocArea(size_t size) {
	Mutex_Lock(&m_mutex);
	const uint32_t framesNeeded = size / I686_PAGE_SIZE;
	uint32_t freeFrames = 0;
	uint32_t resultIndex = m_lowArenaMinIndex;
	uint32_t currentIndex = m_lowArenaMinIndex;
	while (currentIndex < m_lowArenaMaxIndex) {
		if (i686_PhysicalMM_GetBit(currentIndex)) {
			freeFrames = 0;
			resultIndex = currentIndex + 1;
		} else {
			++freeFrames;
		}
		if (freeFrames >= framesNeeded) {
			i686_PhysicalMM_SetRange(resultIndex * I686_PAGE_SIZE, size);
			if (resultIndex == m_lowArenaMinIndex) {
				m_lowArenaMinIndex += framesNeeded;
			}
			Mutex_Unlock(&m_mutex);
			return resultIndex * I686_PAGE_SIZE;
		}
		++currentIndex;
	}
	Mutex_Unlock(&m_mutex);
	return 0;
}

static void i686_PhysicalMM_FreeFrame(uint32_t frame) {
	uint32_t index = frame / I686_PAGE_SIZE;
	if (index < m_lowArenaMaxIndex) {
		if (index < m_lowArenaMinIndex) {
			m_lowArenaMinIndex = index;
		}
	} else {
		if (index < m_highArenaMinIndex) {
			m_highArenaMinIndex = index;
		}
	}
	if (!i686_PhysicalMM_GetBit(frame / I686_PAGE_SIZE)) {
		KernelLog_ErrorMsg(PHYS_MOD_NAME, "Physical memory corruption detected");
	}
	i686_PhysicalMM_ClearBit(frame / I686_PAGE_SIZE);
}

void HAL_PhysicalMM_UserFreeFrame(uintptr_t frame) {
	Mutex_Lock(&m_mutex);
	i686_PhysicalMM_FreeFrame(frame);
	Mutex_Unlock(&m_mutex);
}
void HAL_PhysicalMM_KernelFreeFrame(uint32_t frame) {
	Mutex_Lock(&m_mutex);
	i686_PhysicalMM_FreeFrame(frame);
	Mutex_Unlock(&m_mutex);
}

void HAL_PhysicalMM_KernelFreeArea(uintptr_t area, size_t size) {
	size = ALIGN_UP(size, I686_PAGE_SIZE);
	Mutex_Lock(&m_mutex);
	for (uint32_t offset = 0; offset < size; offset += I686_PAGE_SIZE) {
		i686_PhysicalMM_FreeFrame(area + offset);
	}
	Mutex_Unlock(&m_mutex);
}

void i686_PhysicalMM_Initialize() {
	Mutex_Initialize(&m_mutex);
	memset(&m_bitmap, 0xff, sizeof(m_bitmap));
	struct i686_Stivale_MemoryMap mmap_buf;
	if (!i686_Stivale_GetMemoryMap(&mmap_buf)) {
		KernelLog_ErrorMsg(PHYS_MOD_NAME, "No memory map present");
	}
	uint32_t max = 0;
	for (uint32_t i = 0; i < mmap_buf.entries_count; ++i) {
		uint32_t entry_type = mmap_buf.entries[i].type;
		if (entry_type == AVAILABLE) {
			if (mmap_buf.entries[i].length + mmap_buf.entries[i].base > 0x100000000ULL ||
				mmap_buf.entries[i].length > 0xffffffffULL || mmap_buf.entries[i].base > 0xffffffffULL) {
				KernelLog_WarnMsg(PHYS_MOD_NAME, "area that is not visible in protected mode was "
												 "found. This area will be ignored\n");
				continue;
			}
			uint32_t len = (uint32_t)(mmap_buf.entries[i].length);
			uint32_t addr = (uint32_t)(mmap_buf.entries[i].base);
			uint32_t alignedAddr = ALIGN_UP(addr, I686_PAGE_SIZE);
			uint32_t alignedLen = ALIGN_DOWN(len + addr, I686_PAGE_SIZE) - alignedAddr;
			if (alignedAddr + alignedLen > max) {
				max = alignedAddr + alignedLen;
			}
			i686_PhysicalMM_ClearRange(alignedAddr, alignedLen);
		}
	}
	i686_PhysicalMM_SetRange(0, ALIGN_UP((uint32_t)&i686_PhysicalMM_KernelEnd, I686_PAGE_SIZE));
	m_memoryLimit = max;
	uint32_t pagesCount = m_memoryLimit / I686_PAGE_SIZE;
	if (m_lowArenaMaxIndex > pagesCount) {
		m_lowArenaMaxIndex = pagesCount;
	}
	if (m_highArenaMaxIndex > pagesCount) {
		m_highArenaMaxIndex = pagesCount;
	}
}

uint32_t i686_PhysicalMM_GetMemorySize() {
	return m_memoryLimit;
}

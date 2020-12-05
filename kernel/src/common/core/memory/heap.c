#include <common/core/proc/mutex.h>
#include <hal/memory/phys.h>
#include <hal/memory/virt.h>

#define BLOCK_SIZE 65536
#define HEAP_SIZE_CLASSES_COUNT 13

struct heap_slub_obj_hdr {
	struct heap_slub_obj_hdr *next;
};

static struct Mutex m_mutex;
static size_t m_sizeClasses[HEAP_SIZE_CLASSES_COUNT] = {16,	  32,	64,	  128,	 256,	512,  1024,
														2048, 4096, 8192, 16384, 32768, 65536};

static struct heap_slub_obj_hdr *m_slubs[HEAP_SIZE_CLASSES_COUNT];

static size_t Heap_GetSizeClass(size_t size) {
	for (size_t i = 0; i < HEAP_SIZE_CLASSES_COUNT; ++i) {
		if (size <= m_sizeClasses[i]) {
			return i;
		}
	}
	return HEAP_SIZE_CLASSES_COUNT;
}

bool Heap_ValidateSlubLists() {
	for (size_t i = 0; i < HEAP_SIZE_CLASSES_COUNT; ++i) {
		struct heap_slub_obj_hdr *hdr = m_slubs[i];
		while (hdr != NULL) {
			hdr = hdr->next;
		}
		ASM volatile("nop");
	}
	return true;
}

static bool Heap_AddObjectToSlubs(size_t index) {
	size_t size = m_sizeClasses[index];
	size_t objects_count = BLOCK_SIZE / size;
	size_t block = HAL_PhysicalMM_KernelAllocArea(BLOCK_SIZE);
	if (block == 0) {
		return false;
	}
	for (size_t i = 0; i < objects_count; ++i) {
		size_t offset = i * size;
		size_t address = HAL_VirtualMM_KernelMappingBase + block + offset;
		struct heap_slub_obj_hdr *block = (struct heap_slub_obj_hdr *)address;
		block->next = m_slubs[index];
		m_slubs[index] = block;
	}
	return true;
}

void Heap_Initialize() {
	Mutex_Initialize(&m_mutex);
	for (size_t i = 0; i < HEAP_SIZE_CLASSES_COUNT; ++i) {
		m_slubs[i] = NULL;
	}
}

void *Heap_AllocateMemory(size_t size) {
	if (size == 0) {
		return NULL;
	}
	Mutex_Lock(&m_mutex);
	size_t size_class = Heap_GetSizeClass(size);
	if (size_class == HEAP_SIZE_CLASSES_COUNT) {
		uintptr_t result = HAL_PhysicalMM_KernelAllocArea(ALIGN_UP(size, HAL_VirtualMM_PageSize));
		if (result == 0) {
			Mutex_Unlock(&m_mutex);
			return NULL;
		} else {
			Mutex_Unlock(&m_mutex);
			return (void *)(result + HAL_VirtualMM_KernelMappingBase);
		}
	}
	if (m_slubs[size_class] == NULL) {
		if (!Heap_AddObjectToSlubs(size_class)) {
			Mutex_Unlock(&m_mutex);
			return NULL;
		}
	}
	struct heap_slub_obj_hdr *result = m_slubs[size_class];
	m_slubs[size_class] = result->next;
	Mutex_Unlock(&m_mutex);
	return result;
}

void Heap_FreeMemory(void *area, size_t size) {
	if (area == NULL) {
		return;
	}
	Mutex_Lock(&m_mutex);
	size_t size_class = Heap_GetSizeClass(size);
	if (size_class == HEAP_SIZE_CLASSES_COUNT) {
		Mutex_Unlock(&m_mutex);
		HAL_PhysicalMM_KernelFreeArea((uintptr_t)area, ALIGN_UP(size, HAL_VirtualMM_PageSize));
		return;
	}
	struct heap_slub_obj_hdr *hdr = (struct heap_slub_obj_hdr *)area;
	hdr->next = m_slubs[size_class];
	m_slubs[size_class] = hdr;
	Mutex_Unlock(&m_mutex);
	return;
}

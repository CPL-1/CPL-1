#include <common/core/proc/mutex.h>
#include <hal/memory/phys.h>
#include <hal/memory/virt.h>

#define BLOCK_SIZE 65536
#define HEAP_SIZE_CLASSES_COUNT 13

static struct Mutex Heap_Mutex;
static USIZE Heap_SizeClasses[HEAP_SIZE_CLASSES_COUNT] = {16,	32,	  64,	128,   256,	  512,	1024,
														  2048, 4096, 8192, 16384, 32768, 65536};

struct heap_slub_obj_hdr {
	struct heap_slub_obj_hdr *next;
};

static struct heap_slub_obj_hdr *slubs[HEAP_SIZE_CLASSES_COUNT];

static USIZE Heap_GetSizeClass(USIZE size) {
	for (USIZE i = 0; i < HEAP_SIZE_CLASSES_COUNT; ++i) {
		if (size <= Heap_SizeClasses[i]) {
			return i;
		}
	}
	return HEAP_SIZE_CLASSES_COUNT;
}

bool Heap_ValidateSlubLists() {
	for (USIZE i = 0; i < HEAP_SIZE_CLASSES_COUNT; ++i) {
		struct heap_slub_obj_hdr *hdr = slubs[i];
		while (hdr != NULL) {
			hdr = hdr->next;
		}
		ASM volatile("nop");
	}
	return true;
}

static bool Heap_AddObjectToSlubs(USIZE index) {
	USIZE size = Heap_SizeClasses[index];
	USIZE objects_count = BLOCK_SIZE / size;
	USIZE block = HAL_PhysicalMM_KernelAllocArea(BLOCK_SIZE);
	if (block == 0) {
		return false;
	}
	for (USIZE i = 0; i < objects_count; ++i) {
		USIZE offset = i * size;
		USIZE address = HAL_VirtualMM_KernelMappingBase + block + offset;
		struct heap_slub_obj_hdr *block = (struct heap_slub_obj_hdr *)address;
		block->next = slubs[index];
		slubs[index] = block;
	}
	return true;
}

void Heap_Initialize() {
	Mutex_Initialize(&Heap_Mutex);
	for (USIZE i = 0; i < HEAP_SIZE_CLASSES_COUNT; ++i) {
		slubs[i] = NULL;
	}
}

void *Heap_AllocateMemory(USIZE size) {
	if (size == 0) {
		return NULL;
	}
	Mutex_Lock(&Heap_Mutex);
	USIZE size_class = Heap_GetSizeClass(size);
	if (size_class == HEAP_SIZE_CLASSES_COUNT) {
		UINTN result = HAL_PhysicalMM_KernelAllocArea(ALIGN_UP(size, HAL_VirtualMM_PageSize));
		if (result == 0) {
			Mutex_Unlock(&Heap_Mutex);
			return NULL;
		} else {
			Mutex_Unlock(&Heap_Mutex);
			return (void *)(result + HAL_VirtualMM_KernelMappingBase);
		}
	}
	if (slubs[size_class] == NULL) {
		if (!Heap_AddObjectToSlubs(size_class)) {
			Mutex_Unlock(&Heap_Mutex);
			return NULL;
		}
	}
	struct heap_slub_obj_hdr *result = slubs[size_class];
	slubs[size_class] = result->next;
	Mutex_Unlock(&Heap_Mutex);
	return result;
}

void Heap_FreeMemory(void *area, USIZE size) {
	if (area == NULL) {
		return;
	}
	Mutex_Lock(&Heap_Mutex);
	USIZE size_class = Heap_GetSizeClass(size);
	if (size_class == HEAP_SIZE_CLASSES_COUNT) {
		Mutex_Unlock(&Heap_Mutex);
		HAL_PhysicalMM_KernelFreeArea((UINTN)area, ALIGN_UP(size, HAL_VirtualMM_PageSize));
		return;
	}
	struct heap_slub_obj_hdr *hdr = (struct heap_slub_obj_hdr *)area;
	hdr->next = slubs[size_class];
	slubs[size_class] = hdr;
	Mutex_Unlock(&Heap_Mutex);
	return;
}

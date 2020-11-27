#include <common/core/proc/mutex.h>
#include <hal/memory/phys.h>
#include <hal/memory/virt.h>

#define BLOCK_SIZE 65536
#define HEAP_SIZE_CLASSES_COUNT 13

static struct mutex heap_mutex;
static size_t heap_size_classes[HEAP_SIZE_CLASSES_COUNT] = {
	16, 32, 64, 128, 256, 512, 1024, 2048, 4096, 8192, 16384, 32768, 65536};

struct heap_slub_obj_hdr {
	struct heap_slub_obj_hdr *next;
};

static struct heap_slub_obj_hdr *slubs[HEAP_SIZE_CLASSES_COUNT];

static size_t heap_get_size_class(size_t size) {
	for (size_t i = 0; i < HEAP_SIZE_CLASSES_COUNT; ++i) {
		if (size <= heap_size_classes[i]) {
			return i;
		}
	}
	return HEAP_SIZE_CLASSES_COUNT;
}

bool heap_validate_slub_lists() {
	for (size_t i = 0; i < HEAP_SIZE_CLASSES_COUNT; ++i) {
		struct heap_slub_obj_hdr *hdr = slubs[i];
		while (hdr != NULL) {
			hdr = hdr->next;
		}
		asm volatile("nop");
	}
	return true;
}

static bool heap_add_objects_to_slubs(size_t index) {
	size_t size = heap_size_classes[index];
	size_t objects_count = BLOCK_SIZE / size;
	size_t block = hal_phys_krnl_alloc_area(BLOCK_SIZE);
	if (block == 0) {
		return false;
	}
	for (size_t i = 0; i < objects_count; ++i) {
		size_t offset = i * size;
		size_t address = hal_virt_kernel_mapping_base + block + offset;
		struct heap_slub_obj_hdr *block = (struct heap_slub_obj_hdr *)address;
		block->next = slubs[index];
		slubs[index] = block;
	}
	return true;
}

void heap_init() {
	mutex_init(&heap_mutex);
	for (size_t i = 0; i < HEAP_SIZE_CLASSES_COUNT; ++i) {
		slubs[i] = NULL;
	}
}

void *heap_alloc(size_t size) {
	if (size == 0) {
		return NULL;
	}
	mutex_lock(&heap_mutex);
	size_t size_class = heap_get_size_class(size);
	if (size_class == HEAP_SIZE_CLASSES_COUNT) {
		uintptr_t result =
			hal_phys_krnl_alloc_area(ALIGN_UP(size, hal_virt_page_size));
		if (result == 0) {
			mutex_unlock(&heap_mutex);
			return NULL;
		} else {
			mutex_unlock(&heap_mutex);
			return (void *)(result + hal_virt_kernel_mapping_base);
		}
	}
	if (slubs[size_class] == NULL) {
		if (!heap_add_objects_to_slubs(size_class)) {
			mutex_unlock(&heap_mutex);
			return NULL;
		}
	}
	struct heap_slub_obj_hdr *result = slubs[size_class];
	slubs[size_class] = result->next;
	mutex_unlock(&heap_mutex);
	return result;
}

void heap_free(void *area, size_t size) {
	if (area == NULL) {
		return;
	}
	mutex_lock(&heap_mutex);
	size_t size_class = heap_get_size_class(size);
	if (size_class == HEAP_SIZE_CLASSES_COUNT) {
		mutex_unlock(&heap_mutex);
		hal_phys_krnl_free_area((uintptr_t)area,
								ALIGN_UP(size, hal_virt_page_size));
		return;
	}
	struct heap_slub_obj_hdr *hdr = (struct heap_slub_obj_hdr *)area;
	hdr->next = slubs[size_class];
	slubs[size_class] = hdr;
	mutex_unlock(&heap_mutex);
	return;
}

#include <memory/heap.h>
#include <memory/phys.h>

#define BLOCK_SIZE 16 * PAGE_SIZE

static uint32_t heap_size_classes[] = {
    16, 32, 64, 128, 256, 512, 1024, 2048, 4096, 8192, 16384, 32768, 65536};
static const size_t heap_size_classes_count = ARR_SIZE(heap_size_classes);

struct heap_slub_obj_hdr {
	struct heap_slub_obj_hdr *next;
};

static struct heap_slub_obj_hdr *slubs[heap_size_classes_count];

static uint32_t heap_get_size_class(uint32_t size) {
	for (size_t i = 0; i < heap_size_classes_count; ++i) {
		if (size < heap_size_classes[i]) {
			return i;
		}
	}
	return heap_size_classes_count;
}

static bool heap_add_objects_to_slubs(uint32_t index) {
	uint32_t size = heap_size_classes[index];
	uint32_t objects_count = BLOCK_SIZE / size;
	uint32_t block = phys_lo_alloc_area(BLOCK_SIZE);
	if (block == 0) {
		return false;
	}
	for (size_t i = 0; i < objects_count; ++i) {
		size_t offset = i * size;
		uint32_t address = KERNEL_MAPPING_BASE + block + offset;
		struct heap_slub_obj_hdr *block = (struct heap_slub_obj_hdr *)address;
		block->next = slubs[index];
		slubs[index] = block;
	}
	return true;
}

void heap_init() {
	for (size_t i = 0; i < heap_size_classes_count; ++i) {
		slubs[i] = NULL;
	}
}

void *heap_alloc(uint32_t size) {
	if (size == 0) {
		return NULL;
	}
	uint32_t size_class = heap_get_size_class(size);
	if (size_class == heap_size_classes_count) {
		uint32_t result = phys_lo_alloc_area(ALIGN_UP(size, PAGE_SIZE));
		if (result == 0) {
			return NULL;
		} else {
			return (void *)(result + KERNEL_MAPPING_BASE);
		}
	}
	if (slubs[size_class] == NULL) {
		if (!heap_add_objects_to_slubs(size_class)) {
			return NULL;
		}
	}
	struct heap_slub_obj_hdr *result = slubs[size_class];
	slubs[size_class] = result->next;
	return result;
}

void heap_free(void *area, uint32_t size) {
	if (area == NULL) {
		return;
	}
	uint32_t size_class = heap_get_size_class(size);
	if (size_class == heap_size_classes_count) {
		phys_lo_free_area((uint32_t)area, ALIGN_UP(size, PAGE_SIZE));
	}
	struct heap_slub_obj_hdr *hdr = (struct heap_slub_obj_hdr *)area;
	hdr->next = slubs[size_class];
	slubs[size_class] = hdr;
}

#include <common/misc/platform.h>
#include <common/misc/utils.h>
#include <errno.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <sys/libsyscall.h>

#define BLOCK_SIZE 65536
#define HEAP_SIZE_CLASSES_COUNT 13
#define OBJECTS_MAX (BLOCK_SIZE / 16)

struct Heap_Slub {
	size_t objectsFree;
	size_t sizeClass;
	size_t maxCount;
	struct Heap_Slub *prev;
	struct Heap_Slub *next;
	struct Heap_ObjHeader *first;
};

struct Heap_ObjHeader {
	struct Heap_Slub *slub;
	union {
		struct Heap_ObjHeader *next;
		size_t effectiveSize;
	};
};

static size_t __Heap_SizeClasses[HEAP_SIZE_CLASSES_COUNT] = {16, 32, 64, 128, 256, 512, 1024, 2048, 4096};
static struct Heap_Slub *Heap_Slubs[HEAP_SIZE_CLASSES_COUNT];

static void __Heap_RemoveSlubFromList(struct Heap_Slub *slub) {
	if (slub->next != NULL) {
		slub->next->prev = slub->prev;
	}
	if (slub->prev != NULL) {
		slub->prev->next = slub->next;
	} else {
		Heap_Slubs[slub->sizeClass] = slub->next;
	}
}

static void __Heap_InsertSlubInList(struct Heap_Slub *slub) {
	slub->next = Heap_Slubs[slub->sizeClass];
	if (slub->next != NULL) {
		slub->next->prev = slub;
	}
	Heap_Slubs[slub->sizeClass] = slub;
	slub->prev = NULL;
}

static size_t __Heap_GetSizeClass(size_t size) {
	for (size_t i = 0; i < HEAP_SIZE_CLASSES_COUNT; ++i) {
		if (size <= __Heap_SizeClasses[i]) {
			return i;
		}
	}
	return HEAP_SIZE_CLASSES_COUNT;
}

static uintptr_t __Heap_AllocAnon(size_t size) {
	uintptr_t result =
		(uintptr_t)mmap(NULL, ALIGN_UP(size, __Platform_PageSize), PROT_READ | PROT_WRITE, MAP_ANON, -1, 0);
	if (result == (uintptr_t)-1) {
		return 0;
	}
	return result;
}

static void __Heap_FreeAnon(uintptr_t addr, size_t size) {
	munmap((void *)addr, ALIGN_UP(size, __Platform_PageSize));
}

static bool __Heap_MakeSlub(size_t sizeclass) {
	uintptr_t blk = __Heap_AllocAnon(BLOCK_SIZE);
	if (blk == 0) {
		return false;
	}
	struct Heap_Slub *result = (struct Heap_Slub *)blk;
	result->first = (struct Heap_ObjHeader *)(blk + sizeof(struct Heap_Slub));
	result->objectsFree =
		(BLOCK_SIZE - sizeof(struct Heap_Slub)) / (sizeof(struct Heap_ObjHeader) + __Heap_SizeClasses[sizeclass]);
	result->maxCount = result->objectsFree;
	result->sizeClass = sizeclass;
	size_t step = (sizeof(struct Heap_ObjHeader) + __Heap_SizeClasses[sizeclass]);
	for (size_t i = 0; i < result->objectsFree; ++i) {
		uintptr_t addr = blk + sizeof(struct Heap_Slub) + (step * i);
		uintptr_t next_addr = addr + step;
		struct Heap_ObjHeader *cur = (struct Heap_ObjHeader *)addr;
		struct Heap_ObjHeader *next = (struct Heap_ObjHeader *)next_addr;
		cur->next = next;
		if (i == (result->objectsFree - 1)) {
			cur->next = NULL;
		}
	}
	__Heap_InsertSlubInList(result);
	return true;
}

static struct Heap_ObjHeader *__Heap_GrabFromSlubList(size_t sizeclass) {
	if (Heap_Slubs[sizeclass] == NULL) {
		return NULL;
	}
	struct Heap_Slub *slub = Heap_Slubs[sizeclass];
	struct Heap_ObjHeader *header = slub->first;
	slub->first = header->next;
	slub->objectsFree--;
	if (slub->objectsFree == 0) {
		__Heap_RemoveSlubFromList(slub);
	}
	return header;
}

static struct Heap_ObjHeader *__Heap_AllocateFromSlubs(size_t sizeclass) {
	struct Heap_ObjHeader *result = __Heap_GrabFromSlubList(sizeclass);
	if (result != NULL) {
		return result;
	}
	if (!__Heap_MakeSlub(sizeclass)) {
		return NULL;
	}
	return __Heap_GrabFromSlubList(sizeclass);
}

static struct Heap_ObjHeader *__Heap_Allocate(size_t size) {
	size_t sizeclass = __Heap_GetSizeClass(size);
	if (sizeclass == HEAP_SIZE_CLASSES_COUNT) {
		size_t effectiveSize = ALIGN_UP(size + sizeof(struct Heap_ObjHeader), __Platform_PageSize);
		struct Heap_ObjHeader *header = (struct Heap_ObjHeader *)__Heap_AllocAnon(effectiveSize);
		if (header == NULL) {
			return NULL;
		}
		header->effectiveSize = effectiveSize;
		header->slub = NULL;
		return header;
	} else {
		return __Heap_AllocateFromSlubs(sizeclass);
	}
}

static void __Heap_FreeFromSlubs(struct Heap_ObjHeader *hdr) {
	struct Heap_Slub *slub = hdr->slub;
	hdr->next = slub->first;
	slub->first = hdr->next;
	slub->objectsFree++;
	if (slub->objectsFree == 1) {
		__Heap_InsertSlubInList(slub);
	} else if (slub->objectsFree == slub->maxCount) {
		__Heap_RemoveSlubFromList(slub);
		__Heap_FreeAnon((uintptr_t)slub, BLOCK_SIZE);
	}
}

static void __Heap_Free(struct Heap_ObjHeader *hdr) {
	if (hdr->slub == NULL) {
		__Heap_FreeAnon((uintptr_t)hdr, hdr->effectiveSize);
	} else {
		__Heap_FreeFromSlubs(hdr);
	}
}

static size_t __Heap_GetObjectCapacity(struct Heap_ObjHeader *hdr) {
	if (hdr->slub == NULL) {
		return hdr->effectiveSize - sizeof(struct Heap_ObjHeader);
	} else {
		return __Heap_SizeClasses[hdr->slub->sizeClass];
	}
}

void __Heap_Initialize() {
	for (int i = 0; i < HEAP_SIZE_CLASSES_COUNT; ++i) {
		Heap_Slubs[i] = NULL;
	}
}

void *malloc(size_t size) {
	if (size == 0) {
		return NULL;
	}
	struct Heap_ObjHeader *hdr = __Heap_Allocate(size);
	if (hdr == NULL) {
		return NULL;
	}
	return (void *)(((uintptr_t)hdr) + sizeof(struct Heap_ObjHeader));
}

#define PTR_TO_HDR(p) (struct Heap_ObjHeader *)(((uintptr_t)p) - sizeof(struct Heap_ObjHeader))

void free(void *ptr) {
	if (ptr == NULL) {
		return;
	}
	struct Heap_ObjHeader *hdr = PTR_TO_HDR(ptr);
	__Heap_Free(hdr);
}

void *calloc(size_t nmemb, size_t size) {
	void *new_mem = malloc(nmemb * size);
	if (new_mem == NULL) {
		return NULL;
	}
	memset(new_mem, 0, nmemb * size);
	return new_mem;
}

void *realloc(void *ptr, size_t size) {
	if (ptr == NULL) {
		return malloc(size);
	}
	if (size == 0) {
		free(ptr);
		return NULL;
	}
	struct Heap_ObjHeader *hdr = PTR_TO_HDR(ptr);
	size_t capacity = __Heap_GetObjectCapacity(hdr);
	if (size <= capacity) {
		return ptr;
	}
	void *newArea = malloc(size);
	if (newArea == NULL) {
		return NULL;
	}
	memcpy(newArea, ptr, capacity);
	free(ptr);
	return newArea;
}

void *reallocarray(void *ptr, size_t nmemb, size_t size) {
	size_t real_size;
	if (__builtin_umull_overflow(nmemb, size, &real_size)) {
		errno = ENOMEM;
		return NULL;
	}
	return realloc(ptr, real_size);
}
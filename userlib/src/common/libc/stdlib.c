#include <common/misc/platform.h>
#include <common/misc/utils.h>
#include <errno.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <sys/log.h>
#include <sys/syscall.h>

#define BLOCK_SIZE 65536
#define HEAP_SIZE_CLASSES_COUNT 13
#define OBJECTS_MAX (BLOCK_SIZE / 16)
#define MALLOC_CHECKS 1

#if MALLOC_CHECKS
#define RANGES_MAX 65536
struct Heap_Range {
	struct Heap_Slub *slub;
	uintptr_t base;
	size_t size;
};

static struct Heap_Range __Heap_Ranges[RANGES_MAX] = {{.base = 0, .size = 0, .slub = NULL}};
static size_t __Heap_RangesIndexGuess = 0;
static size_t __Heap_MaxIndex = 0;

static struct Heap_Range *__Heap_RegisterRange(uintptr_t ptr, size_t size) {
	for (size_t i = 0; i < __Heap_MaxIndex; ++i) {
		if ((__Heap_Ranges[i].base + __Heap_Ranges[i].size) <= ptr) {
			continue;
		} else if (__Heap_Ranges[i].base >= ptr + size) {
			continue;
		}
		Log_ErrorMsg("Userspace Allocator", "Overlap detected: 0x%p - 0x%p with new interval 0x%p - 0x%p",
					 __Heap_Ranges[i].base, (__Heap_Ranges[i].base + __Heap_Ranges[i].size), ptr, (ptr + size));
	}
	for (; __Heap_RangesIndexGuess < RANGES_MAX; ++__Heap_RangesIndexGuess) {
		if (__Heap_Ranges[__Heap_RangesIndexGuess].base == 0) {
			__Heap_Ranges[__Heap_RangesIndexGuess].base = ptr;
			__Heap_Ranges[__Heap_RangesIndexGuess].size = size;
			if (__Heap_RangesIndexGuess > __Heap_MaxIndex) {
				__Heap_MaxIndex = __Heap_RangesIndexGuess;
			}
			return __Heap_Ranges + __Heap_RangesIndexGuess;
		}
	}
	Log_ErrorMsg("Userspace Allocator", "Failed to allocate range slot for the object. Increase OBJECT_MAX value");
	return NULL;
}

static void __Heap_UnregisterRange(struct Heap_Range *range) {
	size_t index = range - __Heap_Ranges;
	if (index < __Heap_RangesIndexGuess) {
		__Heap_RangesIndexGuess = index;
	}
	if (index == __Heap_MaxIndex) {
		__Heap_MaxIndex = 0;
		for (size_t i = index; i > 0; --i) {
			if (__Heap_Ranges[i].base != 0) {
				__Heap_MaxIndex = i;
			}
		}
	}
	range->base = 0;
	range->size = 0;
	range->slub = NULL;
}
#endif

struct Heap_Slub {
	size_t objectsFree;
	size_t sizeClass;
	size_t maxCount;
	struct Heap_Slub *prev;
	struct Heap_Slub *next;
	struct Heap_ObjHeader *first;
};

struct Heap_ObjHeader {
#if MALLOC_CHECKS
	struct Heap_Range *range;
#else
	struct Heap_Slub *slub;
#endif
	union {
		struct Heap_ObjHeader *next;
		size_t effectiveSize;
	};
};

static struct Heap_Slub *__Heap_GetSlub(struct Heap_ObjHeader *obj) {
#if MALLOC_CHECKS
	return obj->range->slub;
#else
	return obj->slub;
#endif
}

static void __Heap_SetSlub(struct Heap_ObjHeader *obj, struct Heap_Slub *slub) {
#if MALLOC_CHECKS
	obj->range->slub = slub;
#else
	obj->slub = slub;
#endif
}

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

static struct Heap_ObjHeader *__Heap_GrabFromSlubList(size_t sizeclass, struct Heap_Slub **owner) {
	if (Heap_Slubs[sizeclass] == NULL) {
		return NULL;
	}
	struct Heap_Slub *slub = Heap_Slubs[sizeclass];
	*owner = slub;
	struct Heap_ObjHeader *header = slub->first;
	slub->first = header->next;
	slub->objectsFree--;
	if (slub->objectsFree == 0) {
		__Heap_RemoveSlubFromList(slub);
	}
	return header;
}

static struct Heap_ObjHeader *__Heap_AllocateFromSlubs(size_t sizeclass) {
	struct Heap_Slub *owner;
	struct Heap_ObjHeader *result = __Heap_GrabFromSlubList(sizeclass, &owner);
	if (result != NULL) {
#ifdef MALLOC_CHECKS
		result->range =
			__Heap_RegisterRange(((uintptr_t)result) + sizeof(struct Heap_ObjHeader), __Heap_SizeClasses[sizeclass]);
#else
		(void)size;
#endif
		__Heap_SetSlub(result, owner);
		return result;
	}
	if (!__Heap_MakeSlub(sizeclass)) {
		return NULL;
	}
	result = __Heap_GrabFromSlubList(sizeclass, &owner);
#ifdef MALLOC_CHECKS
	result->range =
		__Heap_RegisterRange(((uintptr_t)result) + sizeof(struct Heap_ObjHeader), __Heap_SizeClasses[sizeclass]);
#else
	(void)size;
#endif
	__Heap_SetSlub(result, owner);
	return result;
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
#ifdef MALLOC_CHECKS
		header->range = __Heap_RegisterRange(((uintptr_t)header) + sizeof(struct Heap_ObjHeader), size);
#endif
		__Heap_SetSlub(header, NULL);
		return header;
	} else {
		return __Heap_AllocateFromSlubs(sizeclass);
	}
}

static void __Heap_FreeFromSlubs(struct Heap_ObjHeader *hdr) {
	struct Heap_Slub *slub = __Heap_GetSlub(hdr);
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
#if MALLOC_CHECKS
	struct Heap_Range *range = hdr->range;
	__Heap_UnregisterRange(range);
#endif
	if (__Heap_GetSlub(hdr) == NULL) {
		__Heap_FreeAnon((uintptr_t)hdr, hdr->effectiveSize);
	} else {
		__Heap_FreeFromSlubs(hdr);
	}
}

static size_t __Heap_GetObjectCapacity(struct Heap_ObjHeader *hdr) {
	if (__Heap_GetSlub(hdr) == NULL) {
		return hdr->effectiveSize - sizeof(struct Heap_ObjHeader);
	} else {
		return __Heap_SizeClasses[__Heap_GetSlub(hdr)->sizeClass];
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
	uintptr_t addr = ((uintptr_t)hdr) + sizeof(struct Heap_ObjHeader);
	return (void *)(addr);
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
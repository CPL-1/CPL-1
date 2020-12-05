#ifndef __DYNARRAY_H_INCLUDED__
#define __DYNARRAY_H_INCLUDED__

#include <common/core/memory/heap.h>
#include <common/misc/utils.h>

#define Dynarray(T) T *

struct DynarrayMetadata {
	size_t count, capacity;
	char data[];
};

#define DYNARRAY_GET_METADATA(d) (((struct DynarrayMetadata *)(d)) - 1)

#define DYNARRAY_NEW(T)                                                                                                \
	({                                                                                                                 \
		struct DynarrayMetadata *meta = ALLOC_OBJ(struct DynarrayMetadata);                                            \
		if (meta != NULL) {                                                                                            \
			meta->count = meta->capacity = 0;                                                                          \
		}                                                                                                              \
		(meta == NULL ? NULL : (Dynarray(T))(meta + 1));                                                               \
	})

#define DYNARRAY_GROW(d, new_capacity)                                                                                 \
	({                                                                                                                 \
		AUTO dg_copy = (d);                                                                                            \
		AUTO capacity_copy = (new_capacity);                                                                           \
		struct DynarrayMetadata *old_dynarray = DYNARRAY_GET_METADATA(dg_copy);                                        \
		size_t element_size = sizeof(*dg_copy);                                                                        \
		size_t array_size = element_size * capacity_copy;                                                              \
		size_t dynarray_alloc_size = array_size + sizeof(struct DynarrayMetadata);                                     \
		struct DynarrayMetadata *new_dynarray = (struct DynarrayMetadata *)Heap_AllocateMemory(dynarray_alloc_size);   \
		if (new_dynarray != NULL) {                                                                                    \
			new_dynarray->count = old_dynarray->count;                                                                 \
			new_dynarray->capacity = capacity_copy;                                                                    \
			memcpy(new_dynarray->data, old_dynarray->data, element_size * old_dynarray->count);                        \
		}                                                                                                              \
		new_dynarray;                                                                                                  \
	})

#define DYNARRAY_DISPOSE(d)                                                                                            \
	({                                                                                                                 \
		AUTO di_copy = (d);                                                                                            \
		struct DynarrayMetadata *dynarray = DYNARRAY_GET_METADATA(di_copy);                                            \
		Heap_FreeMemory(dynarray, (sizeof(*di_copy) * dynarray->capacity + sizeof(struct DynarrayMetadata)));          \
	})

#define DYNARRAY_PUSH(d, e)                                                                                            \
	({                                                                                                                 \
		AUTO dp_copy = (d);                                                                                            \
		struct DynarrayMetadata *dynarray = DYNARRAY_GET_METADATA(dp_copy);                                            \
		if (dynarray->count >= dynarray->capacity) {                                                                   \
			size_t new_capacity = (dynarray->capacity == 0 ? 1 : (2 * dynarray->capacity));                            \
			struct DynarrayMetadata *new_dynarray = DYNARRAY_GROW(dp_copy, new_capacity);                              \
			if (new_dynarray != NULL) {                                                                                \
				DYNARRAY_DISPOSE(dp_copy);                                                                             \
				AUTO data = (typeof(dp_copy))(new_dynarray->data);                                                     \
				data[new_dynarray->count] = (e);                                                                       \
				new_dynarray->count++;                                                                                 \
			}                                                                                                          \
			dynarray = new_dynarray;                                                                                   \
		} else {                                                                                                       \
			AUTO data = (typeof(dp_copy))(dynarray->data);                                                             \
			data[dynarray->count] = (e);                                                                               \
			dynarray->count++;                                                                                         \
		}                                                                                                              \
		(typeof(dp_copy))(dynarray == NULL ? NULL : (dynarray + 1));                                                   \
	})

#define DYNARRAY_LENGTH(d)                                                                                             \
	({                                                                                                                 \
		AUTO dl_copy = (d);                                                                                            \
		struct DynarrayMetadata *dynarray = DYNARRAY_GET_METADATA(dl_copy);                                            \
		dynarray->count;                                                                                               \
	})

#define DYNARRAY_POP(d)                                                                                                \
	({                                                                                                                 \
		AUTO dr_copy = (d);                                                                                            \
		struct DynarrayMetadata *meta = DYNARRAY_GET_METADATA(dr_copy);                                                \
		meta->count--;                                                                                                 \
		dr_copy;                                                                                                       \
	})

#define DYNARRAY_SEARCH(d, e)                                                                                          \
	({                                                                                                                 \
		AUTO ds_copy = (d);                                                                                            \
		AUTO e_copy = (e);                                                                                             \
		size_t result = DYNARRAY_LENGTH(ds_copy);                                                                      \
		for (size_t i = 0; i < DYNARRAY_LENGTH(ds_copy); ++i) {                                                        \
			if (ds_copy[i] == e_copy) {                                                                                \
				result = i;                                                                                            \
				break;                                                                                                 \
			}                                                                                                          \
		}                                                                                                              \
		result;                                                                                                        \
	})

#define POintER_DYNARRAY_INSERT_pointer(d, p, iref)                                                                    \
	({                                                                                                                 \
		AUTO dpi_copy = (d);                                                                                           \
		AUTO pi_copy = (p);                                                                                            \
		AUTO iref_copy = (iref);                                                                                       \
		size_t pos = DYNARRAY_SEARCH(dpi_copy, NULL);                                                                  \
		AUTO result = dpi_copy;                                                                                        \
		if (pos != DYNARRAY_LENGTH(dpi_copy)) {                                                                        \
			dpi_copy[pos] = pi_copy;                                                                                   \
		} else {                                                                                                       \
			result = DYNARRAY_PUSH(dpi_copy, pi_copy);                                                                 \
		}                                                                                                              \
		*iref_copy = pos;                                                                                              \
		result;                                                                                                        \
	})

#define POintER_DYNARRAY_REMOVE_POintER(d, index)                                                                      \
	({                                                                                                                 \
		AUTO pdri_copy = (d);                                                                                          \
		AUTO index_copy = (index);                                                                                     \
		AUTO result = pdri_copy;                                                                                       \
		if (index_copy == DYNARRAY_LENGTH(pdri_copy)) {                                                                \
			result = DYNARRAY_POP(d);                                                                                  \
		} else {                                                                                                       \
			pdri_copy[index_copy] = NULL;                                                                              \
		}                                                                                                              \
		result;                                                                                                        \
	})

#endif
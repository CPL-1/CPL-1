#ifndef __DYNARRAY_H_INCLUDED__
#define __DYNARRAY_H_INCLUDED__

#include <common/core/memory/heap.h>
#include <common/misc/utils.h>

#define dynarray(T) T *

struct dynarray_metadata {
	size_t count, capacity;
	char data[];
};

#define dynarray_get_metadata(d) (((struct dynarray_metadata *)(d)) - 1)

#define dynarray_make(T)                                                       \
	({                                                                         \
		struct dynarray_metadata *meta = ALLOC_OBJ(struct dynarray_metadata);  \
		if (meta != NULL) {                                                    \
			meta->count = meta->capacity = 0;                                  \
		}                                                                      \
		(meta == NULL ? NULL : (dynarray(T))(meta + 1));                       \
	})

#define dynarray_grow(d, new_capacity)                                         \
	({                                                                         \
		typeof(d) dg_copy = (d);                                               \
		typeof(new_capacity) capacity_copy = (new_capacity);                   \
		struct dynarray_metadata *old_dynarray =                               \
			dynarray_get_metadata(dg_copy);                                    \
		size_t element_size = sizeof(*dg_copy);                                \
		size_t array_size = element_size * capacity_copy;                      \
		size_t dynarray_alloc_size =                                           \
			array_size + sizeof(struct dynarray_metadata);                     \
		struct dynarray_metadata *new_dynarray =                               \
			(struct dynarray_metadata *)heap_alloc(dynarray_alloc_size);       \
		if (new_dynarray != NULL) {                                            \
			new_dynarray->count = old_dynarray->count;                         \
			new_dynarray->capacity = old_dynarray->capacity;                   \
			memcpy(new_dynarray->data, old_dynarray->data,                     \
				   element_size * old_dynarray->count);                        \
		}                                                                      \
		new_dynarray;                                                          \
	})

#define dynarray_dispose(d)                                                    \
	({                                                                         \
		typeof(d) di_copy = (d);                                               \
		struct dynarray_metadata *dynarray = dynarray_get_metadata(di_copy);   \
		heap_free(dynarray, (sizeof(*di_copy) * dynarray->capacity +           \
							 sizeof(struct dynarray_metadata)));               \
	})

#define dynarray_push(d, e)                                                    \
	({                                                                         \
		typeof(d) dp_copy = (d);                                               \
		struct dynarray_metadata *dynarray = dynarray_get_metadata(dp_copy);   \
		if (dynarray->count >= dynarray->capacity) {                           \
			size_t new_capacity =                                              \
				(dynarray->capacity == 0 ? 1 : (2 * dynarray->capacity));      \
			struct dynarray_metadata *new_dynarray =                           \
				dynarray_grow(dp_copy, new_capacity);                          \
			if (new_dynarray != NULL) {                                        \
				dynarray_dispose(dp_copy);                                     \
				typeof(dp_copy) data = (typeof(dp_copy))(new_dynarray->data);  \
				data[new_dynarray->count] = (e);                               \
				new_dynarray->count++;                                         \
			}                                                                  \
			dynarray = new_dynarray;                                           \
		} else {                                                               \
			typeof(dp_copy) data = (typeof(dp_copy))(dynarray->data);          \
			data[dynarray->count] = (e);                                       \
			dynarray->count++;                                                 \
		}                                                                      \
		(dynarray == NULL ? NULL : (typeof(dp_copy))(dynarray + 1));           \
	})

#define dynarray_len(d)                                                        \
	({                                                                         \
		typeof(d) dl_copy = (d);                                               \
		struct dynarray_metadata *dynarray = dynarray_get_metadata(dl_copy);   \
		dynarray->count;                                                       \
	})

#endif
#ifndef __DYNARRAY_H_INCLUDED__
#define __DYNARRAY_H_INCLUDED__

#include <memory/heap.h>
#include <utils.h>

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
		auto d_copy = (d);                                                     \
		auto capacity_copy = (new_capacity);                                   \
		struct dynarray_metadata *old_dynarray = dynarray_get_metadata(d);     \
		size_t element_size = sizeof(*d_copy);                                 \
		size_t array_size = element_size * capacity_copy;                      \
		size_t dynarray_alloc_size =                                           \
			array_size + sizeof(struct dynarray_metadata);                     \
		struct dynarray_metadata *new_dynarray =                               \
			heap_alloc(dynarray_alloc_size);                                   \
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
		auto d_copy = d;                                                       \
		struct dynarray_metadata *dynarray = dynarray_get_metadata(d_copy);    \
		heap_free(dynarray, (sizeof(*d_copy) * dynarray->count +               \
							 sizeof(struct dynarray_metadata)));               \
	})

#define dynarray_push(d, e)                                                    \
	({                                                                         \
		auto d_copy = (d);                                                     \
		struct dynarray_metadata *dynarray = dynarray_get_metadata(d_copy);    \
		if (dynarray->count >= dynarray->capacity) {                           \
			struct dynarray_metadata *new_dynarray = dynarray_grow(            \
				d_copy,                                                        \
				dynarray->capacity == 0 ? 1 : (2 * dynarray->capacity));       \
			if (new_dynarray != NULL) {                                        \
				dynarray_dispose(d_copy);                                      \
				auto data = (typeof(d_copy))(new_dynarray->data);              \
				data[new_dynarray->count] = (e);                               \
				new_dynarray->count++;                                         \
			}                                                                  \
			dynarray = new_dynarray;                                           \
		} else {                                                               \
			auto data = (typeof(d_copy))(dynarray->data);                      \
			data[dynarray->count] = (e);                                       \
			dynarray->count++;                                                 \
		}                                                                      \
		(dynarray == NULL ? NULL : (typeof(d_copy))(dynarray + 1));            \
	})

#define dynarray_len(d)                                                        \
	({                                                                         \
		auto d_copy = (d);                                                     \
		struct dynarray_metadata *dynarray = dynarray_get_metadata(d_copy);    \
		dynarray->count;                                                       \
	})

#endif
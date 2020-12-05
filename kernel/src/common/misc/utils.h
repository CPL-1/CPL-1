#ifndef __UTILS_H_INCLUDED__
#define __UTILS_H_INCLUDED__

#include <common/lib/printf.h>
#include <common/misc/attributes.h>
#include <common/misc/types.h>
#include <stdarg.h>
#include <stdatomic.h>

static INLINE void *memset(void *ptr, int value, size_t size) {
	char *writable_ptr = (char *)ptr;
	for (size_t i = 0; i < size; ++i) {
		writable_ptr[i] = (char)value;
	}
	return ptr;
}

static INLINE void *memcpy(void *dst, const void *src, size_t size) {
	char *writable_dst = (char *)dst;
	const char *readable_dst = (const char *)src;
	for (size_t i = 0; i < size; ++i) {
		writable_dst[i] = readable_dst[i];
	}
	return dst;
}

static INLINE size_t strlen(const char *str) {
	size_t result = 0;
	while (*str++ != '\0') {
		result++;
	}
	return result;
}

static INLINE bool StringsEqual(const char *str1, const char *str2) {
	size_t len = strlen(str1);
	if (strlen(str2) != len) {
		return false;
	}
	for (size_t i = 0; i < len; ++i) {
		if (str1[i] != str2[i]) {
			return false;
		}
	}
	return true;
}

static INLINE size_t GetStringHash(const char *str) {
	size_t result = 5381;
	for (size_t i = 0; str[i] != '\0'; ++i) {
		result = ((result << 5) + result) + ((uint8_t)str[i]);
	}
	return result;
}

#define ATOMIC_INCREMENT(x) __atomic_fetch_add(x, 1, __ATOMIC_SEQ_CST)
#define ATOMIC_DECREMENT(x) __atomic_fetch_add(x, -1, __ATOMIC_SEQ_CST)

#define ALIGN_UP(val, align) ((((val) + (align) - (1)) / (align)) * (align))
#define ALIGN_DOWN(val, align) (((val) / (align)) * (align))
#define ARR_SIZE(val) (sizeof(val) / sizeof(*(val)))

#define SPACESHIP(x, y)                                                                                                \
	({                                                                                                                 \
		AUTO _spaceship_x_copy = (x);                                                                                  \
		AUTO _spaceship_y_copy = (y);                                                                                  \
		int result = 0;                                                                                                \
		if (_spaceship_x_copy < _spaceship_y_copy) {                                                                   \
			result = -1;                                                                                               \
		} else if (_spaceship_x_copy > _spaceship_y_copy) {                                                            \
			result = 1;                                                                                                \
		}                                                                                                              \
		result;                                                                                                        \
	})

#define SPACESHIP_NO_ZERO(x, y)                                                                                        \
	({                                                                                                                 \
		AUTO _spaceship_x_copy = (x);                                                                                  \
		AUTO _spaceship_y_copy = (y);                                                                                  \
		int result = -1;                                                                                               \
		if (_spaceship_x_copy < _spaceship_y_copy) {                                                                   \
			result = -1;                                                                                               \
		} else if (_spaceship_x_copy > _spaceship_y_copy) {                                                            \
			result = 1;                                                                                                \
		}                                                                                                              \
		result;                                                                                                        \
	})

#define INT_MAX +2147483647

#endif

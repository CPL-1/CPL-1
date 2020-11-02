#ifndef __UTILS_H_INCLUDED__
#define __UTILS_H_INCLUDED__

#include <attributes.h>
#include <config.h>
#include <lib/printf.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

inline static void *memset(void *ptr, int value, size_t size) {
	char *writable_ptr = (char *)ptr;
	for (size_t i = 0; i < size; ++i) {
		writable_ptr[i] = (char)value;
	}
	return ptr;
}

inline static void *memcpy(void *dst, const void *src, size_t size) {
	char *writable_dst = (char *)dst;
	const char *readable_dst = (const char *)src;
	for (size_t i = 0; i < size; ++i) {
		writable_dst[i] = readable_dst[i];
	}
	return dst;
}

inline static size_t strlen(char *str) {
	size_t result = 0;
	while (*str++ != 0) {
		result++;
	}
	return result;
}

#define ALIGN_UP(val, align) ((((val) + (align) + 1) / (align)) * (align))
#define ALIGN_DOWN(val, align) ((((val)/(align))*(align))
#define ARR_SIZE(val) (sizeof(val) / sizeof(*(val)))

#endif
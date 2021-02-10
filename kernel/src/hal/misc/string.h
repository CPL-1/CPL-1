#ifndef __HAL_STRING_H_INCLUDED__
#define __HAL_STRING_H_INCLUDED__

#include <common/misc/types.h>

void *memcpy(void *dst, const void *src, size_t count);
void *memset(void *dst, int value, size_t size);

#endif

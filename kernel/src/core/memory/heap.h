#ifndef __HEAP_H_INCLUDED__
#define __HEAP_H_INCLUDED__

#include <utils/utils.h>

void heap_init();
void *heap_alloc(uint32_t size);
void heap_free(void *area, uint32_t size);

#define ALLOC_OBJ(t) (t *)heap_alloc(sizeof(t))
#define FREE_OBJ(p) heap_free(p, sizeof(typeof(*(p))))

#endif

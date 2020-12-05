#ifndef __HEAP_H_INCLUDED__
#define __HEAP_H_INCLUDED__

#include <common/misc/utils.h>

void Heap_Initialize();
void *Heap_AllocateMemory(USIZE size);
void Heap_FreeMemory(void *area, USIZE size);

#define ALLOC_OBJ(t) (t *)Heap_AllocateMemory(sizeof(t))
#define FREE_OBJ(p) Heap_FreeMemory(p, sizeof(typeof(*(p))))

#endif

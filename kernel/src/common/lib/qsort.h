#ifndef __QSORT_H_INCLUDED__
#define __QSORT_H_INCLUDED__

#include <common/misc/utils.h>

typedef bool (*QSort_Comparator)(const void *left, const void *right, const void *ctx);

void QSort(void *array, size_t size, size_t count, QSort_Comparator cmp, const void *ctx);

#endif

#include <common/lib/qsort.h>

static void *QSort_GetAtOffset(void *array, size_t size, size_t index) {
	return (void *)(((uintptr_t)array) + size * index);
}

static void QSort_SwapMemory(void *loc1, void *loc2, size_t size) {
	char *cloc1 = (char *)loc1;
	char *cloc2 = (char *)loc2;
	for (size_t i = 0; i < size; ++i) {
		char tmp = cloc1[i];
		cloc1[i] = cloc2[i];
		cloc2[i] = tmp;
	}
}

static void QSort_SwapEntriesAtPositions(void *array, size_t size, size_t i1, size_t i2) {
	QSort_SwapMemory(QSort_GetAtOffset(array, size, i1), QSort_GetAtOffset(array, size, i2), size);
}

static size_t QSort_partition(void *array, size_t size, size_t l, size_t r, QSort_Comparator cmp, const void *ctx) {
	void *v = QSort_GetAtOffset(array, size, (l + r) / 2);
	size_t i = l;
	size_t j = r;
	while (i <= j) {
		while (cmp(QSort_GetAtOffset(array, size, i), v, ctx)) {
			++i;
		}
		while (cmp(v, QSort_GetAtOffset(array, size, j), ctx)) {
			--j;
		}
		if (i >= j) {
			break;
		}
		QSort_SwapEntriesAtPositions(array, size, i++, j--);
	}
	return j;
}

static void QSort_Recursive(void *array, size_t size, size_t l, size_t r, QSort_Comparator cmp, const void *ctx) {
	if (l < r) {
		size_t c = QSort_partition(array, size, l, r, cmp, ctx);
		QSort_Recursive(array, size, l, c, cmp, ctx);
		QSort_Recursive(array, size, c + 1, r, cmp, ctx);
	}
}

void QSort(void *array, size_t size, size_t count, QSort_Comparator cmp, const void *ctx) {
	QSort_Recursive(array, size, 0, count - 1, cmp, ctx);
}

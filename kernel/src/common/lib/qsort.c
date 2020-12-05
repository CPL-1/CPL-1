#include <common/lib/qsort.h>

static void *qsort_GetAtOffset(void *array, USIZE size, USIZE index) {
	return (void *)(((UINTN)array) + size * index);
}

static void qsort_SwapMemory(void *loc1, void *loc2, USIZE size) {
	char *cloc1 = (char *)loc1;
	char *cloc2 = (char *)loc2;
	for (USIZE i = 0; i < size; ++i) {
		char tmp = cloc1[i];
		cloc1[i] = cloc2[i];
		cloc2[i] = tmp;
	}
}

static void qsort_SwapEntriesAtPosition(void *array, USIZE size, USIZE i1, USIZE i2) {
	qsort_SwapMemory(qsort_GetAtOffset(array, size, i1), qsort_GetAtOffset(array, size, i2), size);
}

static USIZE qsort_partition(void *array, USIZE size, USIZE l, USIZE r, qsort_comparator_t cmp, const void *ctx) {
	void *v = qsort_GetAtOffset(array, size, (l + r) / 2);
	USIZE i = l;
	USIZE j = r;
	while (i <= j) {
		while (cmp(qsort_GetAtOffset(array, size, i), v, ctx)) {
			++i;
		}
		while (cmp(v, qsort_GetAtOffset(array, size, j), ctx)) {
			--j;
		}
		if (i >= j) {
			break;
		}
		qsort_SwapEntriesAtPosition(array, size, i++, j--);
	}
	return j;
}

static void qsort_Recursive(void *array, USIZE size, USIZE l, USIZE r, qsort_comparator_t cmp, const void *ctx) {
	if (l < r) {
		USIZE c = qsort_partition(array, size, l, r, cmp, ctx);
		qsort_Recursive(array, size, l, c, cmp, ctx);
		qsort_Recursive(array, size, c + 1, r, cmp, ctx);
	}
}

void qsort(void *array, USIZE size, USIZE count, qsort_comparator_t cmp, const void *ctx) {
	qsort_Recursive(array, size, 0, count - 1, cmp, ctx);
}

#include <common/lib/qsort.h>

static void *qsort_at_offset(void *array, size_t size, size_t index) {
	return (void *)(((uintptr_t)array) + size * index);
}

static void qsort_memswap(void *loc1, void *loc2, size_t size) {
	char *cloc1 = (char *)loc1;
	char *cloc2 = (char *)loc2;
	for (size_t i = 0; i < size; ++i) {
		char tmp = cloc1[i];
		cloc1[i] = cloc2[i];
		cloc2[i] = tmp;
	}
}

static void qsort_swap(void *array, size_t size, size_t i1, size_t i2) {
	qsort_memswap(qsort_at_offset(array, size, i1),
				  qsort_at_offset(array, size, i2), size);
}

static size_t qsort_partition(void *array, size_t size, size_t l, size_t r,
							  qsort_comparator_t cmp, const void *ctx) {
	void *v = qsort_at_offset(array, size, (l + r) / 2);
	size_t i = l;
	size_t j = r;
	while (i <= j) {
		while (cmp(qsort_at_offset(array, size, i), v, ctx)) {
			++i;
		}
		while (cmp(v, qsort_at_offset(array, size, j), ctx)) {
			--j;
		}
		if (i >= j) {
			break;
		}
		qsort_swap(array, size, i++, j--);
	}
	return j;
}

static void qsort_rec(void *array, size_t size, size_t l, size_t r,
					  qsort_comparator_t cmp, const void *ctx) {
	if (l < r) {
		size_t c = qsort_partition(array, size, l, r, cmp, ctx);
		qsort_rec(array, size, l, c, cmp, ctx);
		qsort_rec(array, size, c + 1, r, cmp, ctx);
	}
}

void qsort(void *array, size_t size, size_t count, qsort_comparator_t cmp,
		   const void *ctx) {
	qsort_rec(array, size, 0, count - 1, cmp, ctx);
}

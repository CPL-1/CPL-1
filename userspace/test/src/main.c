#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/log.h>

#define MAX_OBJ 16
#define ITERATIONS 2048

size_t Rand_Current = 3847;

size_t Rand_Next() {
	Rand_Current = (Rand_Current * 16843009 + 8263647) % MAX_OBJ;
	return Rand_Current;
}

void Malloc_Test() {
	size_t *pointers[MAX_OBJ] = {NULL};
	size_t sizes[MAX_OBJ] = {0};
	for (size_t i = 0; i < ITERATIONS; ++i) {
		size_t index = Rand_Next();
		if (pointers[index] == NULL) {
			size_t size = Rand_Next() * 100;
			if (size == 0) {
				continue;
			}
			pointers[index] = malloc(size * sizeof(size_t));
			sizes[index] = size;
			printf("pointers[%u] = malloc(%u);\n", index, size * sizeof(size_t));
			if (pointers[index] == NULL) {
				Log_ErrorMsg("Malloc Test Suite", "Out of Memory");
			}
			for (int i = 0; i < size; ++i) {
				pointers[index][i] = index;
			}
		} else {
			for (int i = 0; i < sizes[index]; ++i) {
				if (pointers[index][i] != index) {
					Log_ErrorMsg("Malloc Test Suite", "Memory corruption");
				}
			}
			printf("free(pointers[%d]);\n", index);
			free(pointers[index]);
			pointers[index] = NULL;
		}
	}
	for (size_t i = 0; i < MAX_OBJ; ++i) {
		if (pointers[i] != NULL) {
			printf("free(pointers[%d]);\n", i);
			free(pointers[i]);
		}
	}
}

int main() {
	Malloc_Test();
}
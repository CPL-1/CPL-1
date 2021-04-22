#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/log.h>
#include <sys/wait.h>
#include <unistd.h>

#define MAX_OBJ 64
#define ITERATIONS 65536

size_t Rand_Current = 3847;

size_t Rand_Next() {
	Rand_Current = (Rand_Current * 17 + 19) % MAX_OBJ;
	return Rand_Current;
}

typedef void (*Test_Callback)();

void Malloc_Test() {
	Log_InfoMsg("Malloc Test Suite", "Testing malloc");
	size_t *pointers[MAX_OBJ] = {NULL};
	size_t sizes[MAX_OBJ] = {0};
	for (size_t i = 0; i < ITERATIONS; ++i) {
		size_t index = Rand_Next();
		if (pointers[index] == NULL) {
			size_t size = Rand_Next() * 1000;
			if (size == 0) {
				continue;
			}
			pointers[index] = malloc(size * sizeof(size_t));
			sizes[index] = size;
			// printf("pointers[%u] = malloc(%u);\n", index, size * sizeof(size_t));
			if (pointers[index] == NULL) {
				Log_ErrorMsg("Malloc Test Suite", "Out of Memory");
			}
			for (size_t i = 0; i < size; ++i) {
				pointers[index][i] = index;
			}
		} else {
			for (size_t i = 0; i < sizes[index]; ++i) {
				if (pointers[index][i] != index) {
					Log_ErrorMsg("Malloc Test Suite", "Memory corruption");
				}
			}
			// printf("free(pointers[%d]);\n", index);
			free(pointers[index]);
			pointers[index] = NULL;
		}
	}
	for (size_t i = 0; i < MAX_OBJ; ++i) {
		if (pointers[i] != NULL) {
			// printf("free(pointers[%d]);\n", i);
			free(pointers[i]);
		}
	}
	Log_InitDoneMsg("Malloc Test Suite");
}

#define TESTS_COUNT 1

Test_Callback m_callbacks[TESTS_COUNT] = {Malloc_Test};
int m_runnersPIDs[TESTS_COUNT] = {-1};
int running = 0;

void Tests_WaitForEveryone() {
	while (running != 0) {
		if (wait4(-1, NULL, 0, NULL) < 0) {
			Log_ErrorMsg("Tests", "Wait4 failed");
		}
		running--;
	}
}

int main() {
	// Start all tests
	for (int i = 0; i < TESTS_COUNT; ++i) {
		Log_InfoMsg("Tests", "Starting test runner for test #%u", i);
		int pid = fork();
		if (pid < 0) {
			Log_WarnMsg("Tests", "Fork failed. Waiting for all processes to die...");
			Tests_WaitForEveryone();
			return -1;
		} else if (pid == 0) {
			Log_InfoMsg("Tests", "Test runner #%u up and running", i);
			m_callbacks[i]();
			return 0;
		} else {
			running++;
			m_runnersPIDs[i] = pid;
		}
	}
	Log_InfoMsg("Tests", "Tests launched. Waiting for them to catch up!");
	Tests_WaitForEveryone();
	return 0;
}

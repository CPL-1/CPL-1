#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/libsyscall.h>

static const char *TestSuite_FileReadTest() {
	FILE *file = fopen("/etc/test/h.txt", "r");
	if (file == NULL) {
		return "Failed to open file /etc/test/h.txt";
	}
	char buf[2];
	int result = fread(buf, 1, 2, file);
	if (result != 1) {
		return "Returned value from read is not 1";
	}
	if (buf[0] != 'h') {
		return "Data read from /etc/test/h.txt is incorrect: buf[0] != \'h\'";
	}
	if (fclose(file) < 0) {
		return "Failed to close the file";
	}
	return NULL;
}

bool TestSuite_VerifyMemoryBlock(void *blk, char byte, int size) {
	for (int i = 0; i < size; ++i) {
		if (((char *)blk)[i] != byte) {
			return false;
		}
	}
	return true;
}

const char *TestSuite_AllocateMemoryTest() {
	void *memory1 = malloc(0x100000);
	void *memory2 = malloc(0x100);
	void *memory3 = malloc(0x1000);
	void *memory4 = malloc(0x10000);
	void *memory5 = malloc(0x100);
	void *memory6 = malloc(0x100);
	void *memory7 = malloc(0x1000);
	if (memory1 == NULL || memory2 == NULL || memory3 == NULL || memory4 == NULL || memory5 == NULL ||
		memory6 == NULL || memory7 == NULL) {
		return "Failed to allocate memory";
	}
	memset(memory1, (char)0x69, 0x100000);
	memset(memory2, (char)0x57, 0x100);
	memset(memory3, (char)0x31, 0x1000);
	memset(memory4, (char)0x42, 0x10000);
	memset(memory5, (char)0x01, 0x100);
	memset(memory6, (char)0x02, 0x100);
	memset(memory7, (char)0x03, 0x1000);
	if (!TestSuite_VerifyMemoryBlock(memory1, 0x69, 0x100000)) {
		return "Failed to read/write on allocated memory (block 1)";
	}
	if (!TestSuite_VerifyMemoryBlock(memory2, 0x57, 0x100)) {
		return "Failed to read/write on allocated memory (block 2)";
	}
	if (!TestSuite_VerifyMemoryBlock(memory3, 0x31, 0x1000)) {
		return "Failed to read/write on allocated memory (block 3)";
	}
	if (!TestSuite_VerifyMemoryBlock(memory4, 0x42, 0x10000)) {
		return "Failed to read/write on allocated memory (block 4)";
	}
	if (!TestSuite_VerifyMemoryBlock(memory5, 0x01, 0x100)) {
		return "Failed to read/write on allocated memory (block 5)";
	}
	if (!TestSuite_VerifyMemoryBlock(memory6, 0x02, 0x100)) {
		return "Failed to read/write on allocated memory (block 6)";
	}
	if (!TestSuite_VerifyMemoryBlock(memory7, 0x03, 0x1000)) {
		return "Failed to read/write on allocated memory (block 7)";
	}
	free(memory1);
	free(memory3);
	free(memory5);
	realloc(memory2, 0);
	free(memory4);
	free(memory7);
	memory6 = realloc(memory6, 0x1000);
	if (!TestSuite_VerifyMemoryBlock(memory6, 0x02, 0x100)) {
		return "Failed to read/write on allocated memory (block 8)";
	}
	free(memory6);
	return NULL;
}

const char *TestSuite_AllocateTooMuchMemory() {
	void *memory = malloc(0xFFFF0000);
	if (memory != NULL) {
		return "Allocated block is definetely not large enough for 0xFFFF0000, we don't have that much RAM on the "
			   "system.";
	}
	return NULL;
}

const char *TestSuite_RunningProcessesTest() {
	char const *args[] = {NULL};
	char const *envp[] = {NULL};
	int pid1 = fork();
	if (pid1 == 0) {
		execve("/bin/hello", args, envp);
	} else {
		int receivedPid1 = wait4(-1, NULL, 0, NULL);
		if (receivedPid1 != pid1) {
			return "Value of pid of process 1 from wait4 system call is wrong";
		}
	}
	int pid2 = fork();
	if (pid2 == 0) {
		execve("/bin/hello", args, envp);
	} else {
		int receivedPid2 = wait4(-1, NULL, 0, NULL);
		if (receivedPid2 != pid2) {
			return "Value of pid of process 2 from wait4 system call is wrong";
		}
	}
	return NULL;
}

struct TestRunner_TestCase {
	const char *testCaseName;
	const char *(*testCallback)();
};

struct TestRunner_TestCase cases[] = {{"open + read + close", TestSuite_FileReadTest},
									  {"malloc(VERY_BIG_MEMORY_SIZE)", TestSuite_AllocateTooMuchMemory},
									  {"malloc(SENSIBLE_MEMORY_SIZE)", TestSuite_AllocateMemoryTest},
									  {"fork + execve + wait4", TestSuite_RunningProcessesTest}};

static void TestRunner_ExecuteTestCases() {
	for (size_t i = 0; i < sizeof(cases) / sizeof(*cases); ++i) {
		printf("[ \033[33mINFO\033[39m ]\033[97m Test Runner:\033[39m Running test case \"%s\"\n",
			   cases[i].testCaseName);
		const char *error = cases[i].testCallback();
		if (error == NULL) {
			printf("[ \033[92mOKAY\033[39m ]\033[97m Test Runner:\033[39m Test case \"%s\" completed without any "
				   "errors\n",
				   cases[i].testCaseName);
		} else {
			printf("[ \033[94mFAIL\033[39m ]\033[97m Test Runner: \033[39m Test case \"%s\" failed with error "
				   "\"%s\"\n",
				   cases[i].testCaseName, error);
			break;
		}
	}
}

bool InitProcess_SetupTTYStreams() {
	if (open("/dev/halterm", O_RDONLY) != 0) {
		return false;
	}
	if (open("/dev/halterm", O_WRONLY) != 1) {
		return false;
	}
	if (open("/dev/halterm", O_WRONLY) != 2) {
		return false;
	}
	return true;
}

int main() {
	if (!InitProcess_SetupTTYStreams()) {
		// Let it hang
		while (true) {
			volatile int a;
			a = 1;
			a = a;
		}
	}
	printf("[ \033[33mINFO\033[39m ]\033[97m Test Runner:\033[39m Executing tests in init process test suite...\n");
	TestRunner_ExecuteTestCases();
	return 0;
}
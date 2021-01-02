#include <libcpl1.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

int strlen(const char *str) {
	int result = 0;
	while (str[result] != '\0') {
		++result;
	}
	return result;
}

void fillmem(void *ptr, char val, int size) {
	char *cptr = (char *)ptr;
	for (int i = 0; i < size; ++i) {
		cptr[i] = val;
	}
}

void print(const char *str) {
	write(STDOUT, str, strlen(str));
}

static const char *TestSuite_FileReadTest() {
	int file = open("/etc/test/h.txt", O_RDONLY);
	if (file < 0) {
		return "Failed to open file /etc/test/h.txt";
	}
	char buf[2];
	int result = read(file, buf, 2);
	if (result != 1) {
		return "Returned value from read is not 1";
	}
	if (buf[0] != 'h') {
		return "Data read from /etc/test/h.txt is incorrect: buf[0] != \'h\'";
	}
	if (close(file) < 0) {
		return "Failed to close the file";
	}
	return NULL;
}

const char *TestSuite_AllocateMemoryTest() {
	void *memory = mmap(NULL, 0x100000, PROT_WRITE | PROT_READ, MAP_ANON, -1, 0);
	if (memory == NULL) {
		return "Failed to allocate memory";
	}
	fillmem(memory, (char)0x69, 0x100000);
	for (int i = 0; i < 0x100000; ++i) {
		if (((char *)memory)[i] != (char)0x69) {
			return "Failed to read/write on allocated memory";
		}
	}
	int result = munmap(memory, 0x100000);
	if (result != 0) {
		return "Failed to unmap memory";
	}
	return NULL;
}

const char *TestSuite_AllocateTooMuchMemory() {
	void *memory = mmap(NULL, 0xFFFF0000, PROT_WRITE | PROT_READ, MAP_ANON, -1, 0);
	if (memory != MAP_FAIL) {
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

struct TestRunner_TestCase cases[] = {{"Reading from file", TestSuite_FileReadTest},
									  {"Allocating too much memory", TestSuite_AllocateTooMuchMemory},
									  {"Allocating memory", TestSuite_AllocateMemoryTest},
									  {"Running processes", TestSuite_RunningProcessesTest}};

static void TestRunner_ExecuteTestCases() {
	for (size_t i = 0; i < sizeof(cases) / sizeof(*cases); ++i) {
		print("[ \033[33mINFO\033[39m ]\033[97m Test Runner:\033[39m Running test case \"");
		print(cases[i].testCaseName);
		print("\"\n");
		const char *error = cases[i].testCallback();
		if (error == NULL) {
			print("[ \033[92mOKAY\033[39m ]\033[97m Test Runner:\033[39m Test case \"");
			print(cases[i].testCaseName);
			print("\" completed without any errors!\n");
		} else {
			print("[ \033[94mFAIL\033[39m ]\033[97m Test Runner: \033[39m Test case \"");
			print(cases[i].testCaseName);
			print("\" failed with error \"");
			print(error);
			print("\"\n");
			break;
		}
	}
}

bool InitProcess_SetupTTYStreams() {
	if (open("/dev/tty0", O_RDONLY) != 0) {
		return false;
	}
	if (open("/dev/tty0", O_RDONLY) != 1) {
		return false;
	}
	if (open("/dev/tty0", O_RDONLY) != 2) {
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
	print("\033[92m\n"
		  "              /\\\n"
		  "             <  >\n"
		  "              \\/\n"
		  "              /\\\n"
		  "             /  \\\n"
		  "            /++++\\\n"
		  "           /  \033[94m()\033[92m  \\\n"
		  "           /      \\\n"
		  "          /~`~`~`~`\\\n"
		  "         /  \033[96m()\033[92m  \033[94m()\033[92m  \\\n"
		  "         /          \\\n"
		  "        /*&*&*&*&*&*&\\\n"
		  "       /  \033[94m()\033[92m  \033[96m()\033[92m  \033[94m()\033[92m  \\\n"
		  "       /              \\\n"
		  "      /++++++++++++++++\\\n"
		  "     /  \033[96m()\033[92m  \033[94m()\033[92m  \033[96m()\033[92m  \033[96m()\033[92m  \\\n"
		  "     /                  \\\n"
		  "    /~`~`~`~`~`~`~`~`~`~`\\\n"
		  "   /  \033[96m()\033[92m  \033[96m()\033[92m  \033[96m()\033[92m  \033[96m()\033[92m  "
		  "\033[94m()\033[92m  \\\n"
		  "   /*&*&*&*&*&*&*&*&*&*&*&\\\n"
		  "  /                        \\\n"
		  " /,.,.,.,.,.,.,.,.,.,.,.,.,.\\\n"
		  "            |   |\n"
		  "           |`````|\n"
		  "           \\_____/\t\tHappy New Year!\n"
		  "\033[39m\n\n");
	print("[ \033[33mINFO\033[39m ]\033[97m Test Runner:\033[39m Executing tests in init process test suite...\n");
	TestRunner_ExecuteTestCases();
	return 0;
}
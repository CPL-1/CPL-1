#include <libcpl1.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

static int m_ttyFd;

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
	CPL1_SyscallWrite(m_ttyFd, str, strlen(str));
}

static const char *TestSuite_FileReadTest() {
	int file = CPL1_SyscallOpen("/etc/test/h.txt", O_RDONLY);
	if (file < 0) {
		return "Failed to open file /etc/test/h.txt";
	}
	char buf[2];
	int result = CPL1_SyscallRead(file, buf, 2);
	if (result != 1) {
		return "Returned value from CPL1_SyscallRead is not 1";
	}
	if (buf[0] != 'h') {
		return "Data read from /etc/test/h.txt is incorrect: buf[0] != \'h\'";
	}
	return NULL;
}

const char *TestSuite_AllocateMemoryTest() {
	void *memory = CPL1_SyscallMemoryMap(NULL, 0x100000, PROT_WRITE | PROT_READ, MAP_ANON, -1, 0);
	if (memory == NULL) {
		return "Failed to allocate memory";
	}
	fillmem(memory, (char)0x69, 0x100000);
	for (int i = 0; i < 0x100000; ++i) {
		if (((char *)memory)[i] != (char)0x69) {
			return "Failed to read/write on allocated memory";
		}
	}
	int result = CPL1_SyscallMemoryUnmap(memory, 0x100000);
	if (result != 0) {
		return "Failed to unmap memory";
	}
	return NULL;
}

const char *TestSuite_AllocateTooMuchMemory() {
	void *memory = CPL1_SyscallMemoryMap(NULL, 0xFFFF0000, PROT_WRITE | PROT_READ, MAP_ANON, -1, 0);
	if (memory != NULL) {
		return "Allocated block is definetely not large enough for 0xFFFF0000, we don't have that much RAM on the "
			   "system.";
	}
	return NULL;
}

struct TestRunner_TestCase {
	const char *testCaseName;
	const char *(*testCallback)();
};

struct TestRunner_TestCase cases[] = {{"Reading from file", TestSuite_FileReadTest},
									  {"Allocating too much memory", TestSuite_AllocateTooMuchMemory},
									  {"Allocating memory", TestSuite_AllocateMemoryTest}};

static void TestRunner_ExecuteTestCases() {
	for (size_t i = 0; i < sizeof(cases) / sizeof(*cases); ++i) {
		print("[ \033[91mINFO\033[39m ]\033[97m Test Runner:\033[39m Running test case \"");
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

int main() {
	m_ttyFd = CPL1_SyscallOpen("/dev/tty0", 0);
	if (m_ttyFd < 0) {
		return -1;
	}
	print("\033[92m\n"
		  "  /$$$$$$  /$$$$$$$  /$$         /$$  \n"
		  " /$$__  $$| $$__  $$| $$       /$$$$  \n"
		  "| $$  \\__/| $$  \\ $$| $$      |_  $$  \n"
		  "| $$      | $$$$$$$/| $$ /$$$$$$| $$  \n"
		  "| $$      | $$____/ | $$|______/| $$  \n"
		  "| $$    $$| $$      | $$        | $$  \n"
		  "|  $$$$$$/| $$      | $$$$$$$$ /$$$$$$\n"
		  " \\______/ |__/      |________/|______/"
		  "\033[39m\n\n");
	print("[ \033[91mINFO\033[39m ]\033[97m Test Runner:\033[39m Executing tests in init process test suite...\n");
	TestRunner_ExecuteTestCases();
	return 0;
}
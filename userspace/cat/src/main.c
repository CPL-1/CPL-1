#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <sys/log.h>
#include <sys/syscall.h>

void Cat_PrintVersion() {
	printf("cat. Copyright (C) 2021 Zamiatin Iurii and CPL-1 contributors\n");
	printf("This program comes with ABSOLUTELY NO WARRANTY; for details type \"cat --license\"\n");
	printf("This is free software, and you are welcome to redistribute it\n");
	printf("under certain conditions; type \"cat --license\" for details.\n");
}

void Cat_PrintHelp() {
	printf("usage: cat [file1] [file2] [file3] ...\n");
}

void Cat_PrintFile(const char *path) {
	int file = open(path, O_RDONLY);
	if (file < 0) {
		Log_WarnMsg("\"cat\" Utility", "Failed to open file \"%s\"", path);
		return;
	}
	char buf[4096];
	int result;
	bool printedAnything = false;
	while ((result = read(file, buf, 4095)) != 0) {
		printedAnything = true;
		if (result < 0) {
			Log_WarnMsg("\"cat\" Utility", "Failed to finish printing the file \"%s\"", path);
			close(file);
			return;
		}
		buf[result] = '\0';
		puts(buf);
	}
	if (printedAnything) {
		puts("\n");
	}
	close(file);
}

void Cat_PrintLicense() {
	char buf[40000];
	int licenseFd = open("/etc/src/COPYING", O_RDONLY);
	if (licenseFd < 0) {
		Log_ErrorMsg("\"ls\" Utility", "Failed to read license from \"/etc/src/COPYING\"");
	}
	int bytes = read(licenseFd, buf, 40000);
	if (bytes < 0) {
		Log_ErrorMsg("\"ls\" Utility", "Failed to read license from \"/etc/src/COPYING\"");
	}
	buf[bytes] = '\0';
	printf("%s\n", buf);
}

int main(int argc, char const *argv[]) {
	if (argc == 2 && (strcmp(argv[1], "--version") == 0 || strcmp(argv[1], "-v") == 0)) {
		Cat_PrintVersion();
		return 0;
	}
	if (argc == 2 && (strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "-h") == 0)) {
		Cat_PrintHelp();
		return 0;
	}
	if (argc == 2 && (strcmp(argv[1], "--license") == 0)) {
		Cat_PrintLicense();
		return 0;
	}
	for (int i = 1; i < argc; ++i) {
		Cat_PrintFile(argv[i]);
	}
	return 0;
}
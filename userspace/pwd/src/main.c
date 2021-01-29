#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <sys/log.h>
#include <sys/syscall.h>

void Pwd_PrintVersion() {
	printf("pwd. Copyright (C) 2021 Zamiatin Iurii and CPL-1 contributors\n");
	printf("This program comes with ABSOLUTELY NO WARRANTY; for details type \"echo --license\"\n");
	printf("This is free software, and you are welcome to redistribute it\n");
	printf("under certain conditions; type \"echo --license\" for details.\n");
}

void Pwd_PrintHelp() {
	printf("pwd - prints current directory name\n");
	printf("usage: pwd\n");
}

void Pwd_PrintLicense() {
	char buf[16536];
	int licenseFd = open("/etc/src/COPYING", O_RDONLY);
	if (licenseFd < 0) {
		Log_ErrorMsg("\"pwd\" Utility", "Failed to read license from \"/etc/src/COPYING\"");
	}
	int bytes = read(licenseFd, buf, 16535);
	if (bytes < 0) {
		Log_ErrorMsg("\"pwd\" Utility", "Failed to read license from \"/etc/src/COPYING\"");
	}
	buf[bytes] = '\0';
	printf("%s\n", buf);
}

int main(int argc, char const *argv[]) {
	if (argc == 2 && (strcmp(argv[1], "--version") == 0 || strcmp(argv[1], "-v") == 0)) {
		Pwd_PrintVersion();
		return 0;
	}
	if (argc == 2 && (strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "-h") == 0)) {
		Pwd_PrintHelp();
		return 0;
	}
	if (argc == 2 && (strcmp(argv[1], "--license") == 0)) {
		Pwd_PrintLicense();
		return 0;
	}
	char pwd[8192];
	int result = getcwd(pwd, 8192);
	if (result < 0) {
		Log_ErrorMsg("\"pwd\" Utility", "Failed to get current working directory path");
		return -1;
	}
	printf("%s\n", pwd);
	return 0;
}
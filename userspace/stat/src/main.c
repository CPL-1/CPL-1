#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <sys/log.h>
#include <sys/syscall.h>

void Stat_PrintVersion() {
	printf("stat. Copyright (C) 2021 Zamiatin Iurii and CPL-1 contributors\n");
	printf("This program comes with ABSOLUTELY NO WARRANTY; for details type \"echo --license\"\n");
	printf("This is free software, and you are welcome to redistribute it\n");
	printf("under certain conditions; type \"echo --license\" for details.\n");
}

void Stat_PrintHelp() {
	printf("usage: stat filename\n");
	printf("Prints information about the file\n");
}

void Stat_PrintLicense() {
	char buf[40000];
	int licenseFd = open("/etc/src/COPYING", O_RDONLY);
	if (licenseFd < 0) {
		Log_ErrorMsg("\"stat\" Utility", "Failed to read license from \"/etc/src/COPYING\"");
	}
	int bytes = read(licenseFd, buf, 40000);
	if (bytes < 0) {
		Log_ErrorMsg("\"stat\" Utility", "Failed to read license from \"/etc/src/COPYING\"");
	}
	buf[bytes] = '\0';
	printf("%s\n", buf);
}

const char *Stat_FileTypes[] = {
	"unknown",		"fifo",			 "character device", "unknown", "directory", "unknown", "block device", "unknown",
	"regular file", "symbolic link", "unknown",			 "unknown", "socket",	 "unknown", "unknown",
};

int main(int argc, char const *argv[]) {
	if (argc == 2 && (strcmp(argv[1], "--version") == 0 || strcmp(argv[1], "-v") == 0)) {
		Stat_PrintVersion();
		return 0;
	}
	if (argc == 2 && (strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "-h") == 0)) {
		Stat_PrintHelp();
		return 0;
	}
	if (argc == 2 && (strcmp(argv[1], "--license") == 0)) {
		Stat_PrintLicense();
		return 0;
	}
	if (argc != 2) {
		Stat_PrintHelp();
		return -1;
	}
	const char *path = argv[1];
	int fd = open(path, O_RDONLY);
	if (fd < 0) {
		Log_ErrorMsg("\"stat\" Utility", "Failed to open the file \"%s\"", path);
	}
	struct stat stat;
	if (fstat(fd, &stat) < 0) {
		Log_ErrorMsg("\"stat\" Utility", "Failed to read information about the file \"%s\"", path);
	}
	printf("\tFile: %s\n", path);
	const char *type = "unknown";
	if (stat.stType < 15) {
		type = Stat_FileTypes[stat.stType];
	}
	printf("\tSize: %u    Blocks: %u    IO Block: %u    %s\n", (uint32_t)stat.stSize, (uint32_t)stat.stBlkcnt,
		   (uint32_t)stat.stBlksize, type);
	return 0;
}
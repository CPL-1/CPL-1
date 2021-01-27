#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <sys/log.h>
#include <sys/syscall.h>

void Ls_ListDirectory(const char *dir) {
	int fd = open(dir, O_RDONLY);
	if (fd < 0) {
		Log_ErrorMsg("Directory enumerator", "Failed to open directory \"%s\"", dir);
	}
	struct dirent buf;
	bool first = true;
	while (true) {
		int result = getdents(fd, &buf, 1);
		if (result < 0) {
			if (!first) {
				puts("\n");
			}
			Log_ErrorMsg("Directory enumerator", "Error occured while enumerating the directory \"%s\"", dir);
		}
		if (result == 0) {
			break;
		}
		if (strcmp(buf.d_name, "..") == 0 || strcmp(buf.d_name, ".") == 0) {
			continue;
		}
		printf("%s ", buf.d_name);
		first = false;
	}
}

int main(int argc, char const *argv[]) {
	if (argc < 2) {
		Ls_ListDirectory(".");
		puts("\b\n");
	} else if (argc == 2) {
		Ls_ListDirectory(argv[1]);
		puts("\b\n");
	} else {
		for (int i = 1; i < argc; ++i) {
			printf("%s:\n\t", argv[i]);
			Ls_ListDirectory(argv[i]);
			if (i < argc - 1) {
				puts("\b\n\n");
			} else {
				puts("\b\n");
			}
		}
	}
	return 0;
}
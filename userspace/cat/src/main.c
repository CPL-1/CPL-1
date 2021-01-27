#include <stdbool.h>
#include <stdio.h>
#include <sys/log.h>
#include <sys/syscall.h>

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

int main(int argc, char const *argv[]) {
	for (int i = 1; i < argc; ++i) {
		Cat_PrintFile(argv[i]);
	}
	return 0;
}
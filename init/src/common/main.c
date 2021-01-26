#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/log.h>
#include <sys/syscall.h>

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

void logo() {
	int logo = open("/etc/init/logo.txt", O_RDONLY);
	if (logo < 0) {
		Log_ErrorMsg("Init Process", "Failed to display logo =(");
	}
	char buf[4097];
	int count = read(logo, buf, 4096);
	buf[count] = '\0';
	printf("%s", buf);
}

int main() {
	if (!InitProcess_SetupTTYStreams()) {
		while (true) {
		}
	}
	logo();
	Log_InfoMsg("Init Process", "Up and running. Starting shell");
	while (true) {
		int pid = fork();
		if (pid == 0) {
			char const *argv[] = {NULL};
			char const *envp[] = {NULL};
			execve("/bin/sh", argv, envp);
			printf("failed to start shell");
			exit(-1);
		} else {
			wait4(-1, NULL, 0, NULL);
		}
		Log_WarnMsg("Init Process", "Shell terminated. Restarting");
	}
	return 0;
}
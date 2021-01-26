#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <sys/log.h>
#include <sys/syscall.h>

int main(int argc, char const *argv[], char const *envp[]) {
	(void)argv;
	(void)argc;
	int wstatus;
	while (true) {
		// Step 1. User input
		char buf[4096];
		puts("$ ");
		int inputSize = read(0, buf, 4095);
		if (inputSize < 0) {
			Log_ErrorMsg("Shell", "Failed to read from /dev/halterm");
		} else if (inputSize == 0) {
			continue;
		} else {
			buf[inputSize - 1] = '\0';
		}
		// Step 2. Argument parsing
		const char *argv[4097];
		int argc = 0;
		for (int i = 0; i < inputSize; ++i) {
			if (buf[i] != ' ') {
				bool scoped = (buf[i] == '\'' || buf[i] == '\"');
				char start = buf[i];
				argv[argc++] = buf + (i + (scoped ? 1 : 0));
				++i;
				bool found = false;
				while (i < inputSize) {
					if (scoped) {
						if (buf[i] == start) {
							found = true;
							break;
						}
					} else {
						if (buf[i] == ' ') {
							break;
						}
					}
					++i;
				}
				if ((!found) && scoped) {
					argc--;
				}
				buf[i] = '\0';
			}
		}
		argv[argc] = NULL;
		// Handle a few builtins
		if (strcmp(argv[0], "help") == 0) {
			printf("CPL-1 shell, version v0.0.1\n");
			printf("These shell commands are defined internally\n");
			printf("\"help\" - output this message\n");
			continue;
		}
		// Step 3. Execution
		if (argc == 0) {
			continue;
		}
		int pid = fork();
		if (pid == 0) {
			execve(argv[0], argv + 1, envp);
			Log_ErrorMsg("Shell", "Failed to start \"%s\"", argv[0]);
		} else {
			int result = wait4(-1, &wstatus, 0, NULL);
			if (result != pid) {
				Log_ErrorMsg("Shell", "Unexpected returned PID");
			}
		}
	}
}
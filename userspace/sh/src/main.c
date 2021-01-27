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
	start:
		puts("\033[92mroot\033[39m# \033[97m");
		int inputSize = read(0, buf, 4095);
		puts("\033[39m");
		if (inputSize < 0) {
			Log_ErrorMsg("Shell", "Failed to read from /dev/halterm");
		} else if (inputSize == 0) {
			continue;
		} else {
			buf[inputSize - 1] = '\0';
			inputSize--;
		}
		// Step 2. Argument parsing
		const char *argv[4097];
		int argc = 0;
		for (int i = 0; i < inputSize; ++i) {
		mainLoop:
			if (i >= inputSize) {
				break;
			}
			if (buf[i] != ' ' && buf[i] != '\n') {
				if (buf[i] == '\'' || buf[i] == '\"') {
					char sep = buf[i];
					++i;
					if (i == inputSize) {
						Log_WarnMsg("Shell", "Syntax error. Try entering your command again");
						goto start;
					}
					argv[argc++] = buf + i;
					for (; i < inputSize; ++i) {
						if (buf[i] == sep) {
							buf[i++] = '\0';
							goto mainLoop;
						}
					}
					Log_WarnMsg("Shell", "Syntax error. Try entering your command again");
					goto start;
				} else {
					argv[argc++] = buf + i;
					for (; i < inputSize; ++i) {
						if (buf[i] == ' ') {
							buf[i++] = '\0';
							goto mainLoop;
						}
					}
				}
			}
		}
		argv[argc] = NULL;
		// Step 3. Handle builtins
		if (strcmp(argv[0], "help") == 0) {
			printf("CPL-1 shell, version v0.0.1\n");
			printf("These shell commands are defined internally\n");
			printf("\"help\" - output this message\n");
			printf("\"exit\" - exit shell\n");
			continue;
		} else if (strcmp(argv[0], "exit") == 0) {
			exit(0);
		}
		// Step 4. Parse name
		if (argv[0] == NULL || *(argv[0]) == '\0') {
			continue;
		}
		char filename_buf[4096];
		const char *filename = argv[0];
		if (*(argv[0]) != '/') {
			snprintf(filename_buf, 4095, "/bin/%s", argv[0]);
			filename = filename_buf;
		}
		// Step 5. Execution
		if (argc == 0) {
			continue;
		}
		int pid = fork();
		if (pid == 0) {
			execve(filename, argv, envp);
			Log_ErrorMsg("Shell", "Failed to start \"%s\"", filename);
		} else {
			int result = wait4(-1, &wstatus, 0, NULL);
			if (result != pid) {
				Log_ErrorMsg("Shell", "Unexpected returned PID");
			}
		}
	}
	return 0;
}
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <sys/log.h>
#include <sys/syscall.h>

void Shell_PrintVersion() {
	printf("sh. Copyright (C) 2021 Zamiatin Iurii and CPL-1 contributors\n");
	printf("This program comes with ABSOLUTELY NO WARRANTY; for details type \"sh --license\"\n");
	printf("This is free software, and you are welcome to redistribute it\n");
	printf("under certain conditions; type \"sh --license\" for details.\n");
}

void Shell_PrintHelp() {
	printf("These commands are builtin\n");
	printf("* \"help\" - output this message\n");
	printf("* \"exit\" - exit shell\n");
	printf("To start the program, type its full absolute path.\n");
	printf("If the file is in /bin directory, just type its name.\n");
	printf("Arguments are typed right after the command name.\n");
	printf("They are separated either with spaces (\"ls /bin\"). Arguments can be quoted with \'...\' or \"...\" if "
		   "whitespace needs to be included.\n");
}

void Shell_PrintLicense() {
	char buf[16536];
	int licenseFd = open("/etc/src/COPYING", O_RDONLY);
	if (licenseFd < 0) {
		Log_ErrorMsg("\"ls\" Utility", "Failed to read license from \"/etc/src/COPYING\"");
	}
	int bytes = read(licenseFd, buf, 16535);
	if (bytes < 0) {
		Log_ErrorMsg("\"ls\" Utility", "Failed to read license from \"/etc/src/COPYING\"");
	}
	buf[bytes] = '\0';
	printf("%s\n", buf);
}

int main(int argc, char const *argv[], char const *envp[]) {
	(void)argv;
	(void)argc;
	if (argc == 2 && (strcmp(argv[1], "--version") == 0 || strcmp(argv[1], "-v") == 0)) {
		Shell_PrintVersion();
		return 0;
	}
	if (argc == 2 && (strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "-h") == 0)) {
		Shell_PrintHelp();
		return 0;
	}
	if (argc == 2 && (strcmp(argv[1], "--license") == 0)) {
		Shell_PrintLicense();
		return 0;
	}
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
		if (argv[0] == NULL) {
			continue;
		}
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
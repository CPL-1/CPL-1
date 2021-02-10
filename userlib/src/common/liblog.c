#include <stdarg.h>
#include <stdio.h>
#include <sys/log.h>
#include <sys/syscall.h>

void Log_InitDoneMsg(const char *mod) {
	printf("[ \033[92mOKAY\033[39m ] Target \033[97m%s\033[39m reached\n", mod);
}

void Log_OkMsg(const char *mod, const char *fmt, ...) {
	va_list args;
	va_start(args, fmt);
	printf("[ \033[92mOKAY\033[39m ]\033[97m %s: \033[39m", mod);
	va_printf(fmt, args);
	printf("\n");
	va_end(args);
}

void Log_WarnMsg(const char *mod, const char *fmt, ...) {
	va_list args;
	va_start(args, fmt);
	printf("[ \033[96mWARN\033[39m ]\033[97m %s: \033[39m", mod);
	va_printf(fmt, args);
	printf("\n");
	va_end(args);
}

void Log_ErrorMsg(const char *mod, const char *fmt, ...) {
	va_list args;
	va_start(args, fmt);
	printf("[ \033[94mFAIL\033[39m ]\033[97m %s: \033[39m", mod);
	va_printf(fmt, args);
	printf("\n");
	va_end(args);
	exit(-1);
}

void Log_InfoMsg(const char *mod, const char *fmt, ...) {
	va_list args;
	va_start(args, fmt);
	printf("[ \033[33mINFO\033[39m ]\033[97m %s: \033[39m", mod);
	va_printf(fmt, args);
	printf("\n");
	va_end(args);
}

void Log_Print(const char *fmt, ...) {
	va_list args;
	va_start(args, fmt);
	printf("         ");
	va_printf(fmt, args);
	printf("\n");
	va_end(args);
}

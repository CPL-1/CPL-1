#include <common/lib/kmsg.h>
#include <common/misc/attributes.h>
#include <hal/drivers/tty.h>
#include <hal/proc/intlevel.h>

void KernelLog_InitDoneMsg(const char *mod) {
	int level = HAL_InterruptLevel_Elevate();
	printf("[ \033[92mOKAY\033[39m ] Target \033[97m%s\033[39m reached\n", mod);
	HAL_InterruptLevel_Recover(level);
}

void KernelLog_OkMsg(const char *mod, const char *fmt, ...) {
	va_list args;
	va_start(args, fmt);
	int level = HAL_InterruptLevel_Elevate();
	printf("[ \033[92mOKAY\033[39m ]\033[97m %s: \033[39m", mod);
	va_printf(fmt, args);
	printf("\n");
	HAL_InterruptLevel_Recover(level);
	va_end(args);
}

void KernelLog_WarnMsg(const char *mod, const char *fmt, ...) {
	va_list args;
	va_start(args, fmt);
	int level = HAL_InterruptLevel_Elevate();
	printf("[ \033[96mWARN\033[39m ]\033[97m %s: \033[39m", mod);
	va_printf(fmt, args);
	printf("\n");
	HAL_InterruptLevel_Recover(level);
	va_end(args);
}

void KernelLog_ErrorMsg(const char *mod, const char *fmt, ...) {
	va_list args;
	va_start(args, fmt);
	HAL_InterruptLevel_Elevate();
	printf("[ \033[94mFAIL\033[39m ]\033[97m %s: \033[39m", mod);
	va_printf(fmt, args);
	printf("\n");
	while (true) {
	}
	va_end(args);
}

void KernelLog_InfoMsg(const char *mod, const char *fmt, ...) {
	va_list args;
	va_start(args, fmt);
	int level = HAL_InterruptLevel_Elevate();
	printf("[ \033[33mINFO\033[39m ]\033[97m %s: \033[39m", mod);
	va_printf(fmt, args);
	printf("\n");
	HAL_InterruptLevel_Recover(level);
	va_end(args);
}

void KernelLog_Print(const char *fmt, ...) {
	va_list args;
	va_start(args, fmt);
	int level = HAL_InterruptLevel_Elevate();
	printf("         ");
	va_printf(fmt, args);
	printf("\n");
	HAL_InterruptLevel_Recover(level);
	va_end(args);
}
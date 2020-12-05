#include <common/lib/kmsg.h>
#include <common/misc/attributes.h>
#include <hal/drivers/tty.h>
#include <hal/proc/intlock.h>

void KernelLog_InitDoneMsg(const char *mod) {
	HAL_InterruptLock_Lock();
	printf("[ ");
	HAL_TTY_SetColor(0x0a);
	printf("OKAY");
	HAL_TTY_SetColor(0x07);
	printf(" ] Target ");
	HAL_TTY_SetColor(0x0f);
	printf("%s", mod);
	HAL_TTY_SetColor(0x07);
	printf(" reached\n");
	HAL_InterruptLock_Unlock();
}

void KernelLog_OkMsg(const char *mod, const char *fmt, ...) {
	va_list args;
	va_start(args, fmt);
	HAL_InterruptLock_Lock();
	printf("[ ");
	HAL_TTY_SetColor(0x0a);
	printf("OKAY");
	HAL_TTY_SetColor(0x07);
	printf(" ] ");
	HAL_TTY_SetColor(0x0f);
	printf("%s: ", mod);
	HAL_TTY_SetColor(0x07);
	va_printf(fmt, args);
	printf("\n");
	HAL_InterruptLock_Unlock();
	va_end(args);
}

void KernelLog_WarnMsg(const char *mod, const char *fmt, ...) {
	va_list args;
	va_start(args, fmt);
	HAL_InterruptLock_Lock();
	printf("[ ");
	HAL_TTY_SetColor(0x0e);
	printf("WARN");
	HAL_TTY_SetColor(0x07);
	printf(" ] ");
	HAL_TTY_SetColor(0x0f);
	printf("%s: ", mod);
	HAL_TTY_SetColor(0x07);
	va_printf(fmt, args);
	printf("\n");
	HAL_InterruptLock_Unlock();
	va_end(args);
}

void KernelLog_ErrorMsg(const char *mod, const char *fmt, ...) {
	va_list args;
	va_start(args, fmt);
	HAL_InterruptLock_Lock();
	printf("[ ");
	HAL_TTY_SetColor(0x0c);
	printf("FAIL");
	HAL_TTY_SetColor(0x07);
	printf(" ] ");
	HAL_TTY_SetColor(0x0f);
	printf("%s: ", mod);
	HAL_TTY_SetColor(0x07);
	va_printf(fmt, args);
	printf("\n");
	while (true) {
		ASM volatile("nop");
	}
	va_end(args);
}

void KernelLog_InfoMsg(const char *mod, const char *fmt, ...) {
	va_list args;
	va_start(args, fmt);
	HAL_InterruptLock_Lock();
	printf("[ ");
	HAL_TTY_SetColor(0x09);
	printf("INFO");
	HAL_TTY_SetColor(0x07);
	printf(" ] ");
	HAL_TTY_SetColor(0x0f);
	printf("%s: ", mod);
	HAL_TTY_SetColor(0x07);
	va_printf(fmt, args);
	printf("\n");
	HAL_InterruptLock_Unlock();
	va_end(args);
}

void KernelLog_Print(const char *fmt, ...) {
	va_list args;
	va_start(args, fmt);
	HAL_InterruptLock_Lock();
	printf("         ");
	va_printf(fmt, args);
	printf("\n");
	HAL_InterruptLock_Unlock();
	va_end(args);
}
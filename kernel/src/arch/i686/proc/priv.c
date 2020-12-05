#include <arch/i686/cpu/idt.h>
#include <arch/i686/proc/priv.h>

extern void i686_Ring0Executor_InvokeISRHandler();

void i686_Ring0Executor_Initialize() {
	i686_IDT_InstallISR(0xff, (UINT32)i686_Ring0Executor_InvokeISRHandler);
}
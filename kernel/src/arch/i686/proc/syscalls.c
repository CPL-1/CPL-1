#include <arch/i686/proc/syscalls.h>
#include <common/core/proc/proc.h>
#include <common/lib/kmsg.h>

int i686_Syscall_ExitProcess(struct i686_CPUState *state) {
	KernelLog_InfoMsg("i686 ExitProcess System Call", "Terminating process");
	Proc_Exit(state->ebx);
	KernelLog_ErrorMsg("i686 ExitProcess System Call", "Failed to terminate process");
	while (true) {
		asm volatile("nop");
	}
}
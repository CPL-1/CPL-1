#include <arch/i686/cpu/tss.h>
#include <hal/proc/stack.h>

void HAL_ISRStacks_SetISRStack(uintptr_t stack_top) {
	i686_TSS_SetISRStack(stack_top, 0x10);
}

void HAL_ISRStacks_SetSyscallsStack(uintptr_t stack_top) {
	i686_TSS_SetKernelStack(stack_top, 0x21);
}

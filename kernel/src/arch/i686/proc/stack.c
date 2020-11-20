#include <arch/i686/cpu/tss.h>
#include <hal/proc/stack.h>

void hal_stack_isr_set(uintptr_t stack_top) {
	i686_tss_set_dpl0_stack(stack_top, 0x10);
}

void hal_stack_syscall_set(uintptr_t stack_top) {
	i686_tss_set_dpl1_stack(stack_top, 0x21);
}
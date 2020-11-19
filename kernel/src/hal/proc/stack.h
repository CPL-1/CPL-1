#ifndef __HAL_STACK_INCLUDED__
#define __HAL_STACK_INCLUDED__

void hal_stack_isr_set(uintptr_t stack_top);
void hal_stack_syscall_set(uintptr_t stack_top);

#endif
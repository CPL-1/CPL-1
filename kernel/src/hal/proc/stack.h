#ifndef __HAL_STACK_INCLUDED__
#define __HAL_STACK_INCLUDED__

void HAL_ISRStacks_SetISRStack(uintptr_t stack_top);
void HAL_ISRStacks_SetSyscallsStack(uintptr_t stack_top);

#endif

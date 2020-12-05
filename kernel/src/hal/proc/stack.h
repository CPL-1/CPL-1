#ifndef __HAL_STACK_INCLUDED__
#define __HAL_STACK_INCLUDED__

void HAL_ISRStacks_SetISRStack(UINTN stack_top);
void HAL_ISRStacks_SetSyscallsStack(UINTN stack_top);

#endif
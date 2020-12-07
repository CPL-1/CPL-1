#ifndef __I686_SYSCALLS_H_INCLUDED__
#define __I686_SYSCALLS_H_INCLUDED__

#include <arch/i686/proc/state.h>

int i686_Syscall_ExitProcess(struct i686_CPUState *state);

#endif
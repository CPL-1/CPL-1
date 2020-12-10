#ifndef __I686_SYSCALLS_H_INCLUDED__
#define __I686_SYSCALLS_H_INCLUDED__

#include <arch/i686/proc/state.h>

void i686_Syscall_Exit(struct i686_CPUState *state);
void i686_Syscall_Open(struct i686_CPUState *state);
void i686_Syscall_Read(struct i686_CPUState *state);
void i686_Syscall_Write(struct i686_CPUState *state);
void i686_Syscall_Close(struct i686_CPUState *state);
void i686_Syscall_MemoryMap(struct i686_CPUState *state);
void i686_Syscall_MemoryUnmap(struct i686_CPUState *state);
void i686_Syscall_Fork(struct i686_CPUState *state);
void i686_Syscall_Execve(struct i686_CPUState *state);
void i686_Syscall_Wait4(struct i686_CPUState *state);

#endif
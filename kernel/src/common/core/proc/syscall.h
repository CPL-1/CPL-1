#ifndef __SYSCALL_H_INCLUDED__
#define __SYSCALL_H_INCLUDED__

#include <common/misc/utils.h>

/// NOTE: Address space should be unlocked on all those functions

int Syscall_Open(uintptr_t pathAddr, int perms);
int Syscall_Read(int fd, uintptr_t bufAddr, int size);
int Syscall_Write(int fd, uintptr_t bufAddr, int size);
int Syscall_Close(int fd);
uintptr_t Syscall_MemoryMap(uintptr_t addr, size_t size, int prot, int flags);
void Syscall_MemoryUnmap(uintptr_t addr, size_t size);
int Syscall_Fork();
/*
int Syscall_Execve(struct CPUState *state);
int Syscall_Wait4(struct CPUState *state);
int Syscall_GetDirectoryEntries(struct CPUState *state);
int Syscall_Chdir(struct CPUState *state);
int Syscall_Fchdir(struct CPUState *state);
int Syscall_GetCWD(struct CPUState *state);
int Syscall_GetPID(struct CPUState *state);
int Syscall_GetPPID(struct CPUState *state);
int Syscall_Fstat(struct CPUState *state);
*/
#endif

#ifndef __SYSCALL_H_INCLUDED__
#define __SYSCALL_H_INCLUDED__

#include <common/core/proc/proc.h>
#include <common/misc/utils.h>

/// NOTE: Address space should be unlocked on all those functions

int Syscall_Open(uintptr_t pathAddr, int perms);
int Syscall_Read(int fd, uintptr_t bufferAddr, int size);
int Syscall_Write(int fd, uintptr_t bufferAddr, int size);
uintptr_t Syscall_MemoryMap(uintptr_t addr, size_t size, int prot, int flags);
int Syscall_MemoryUnmap(uintptr_t addr, size_t size);
struct Proc_ProcessID Syscall_Fork(void *state);
int Syscall_Execve32(uintptr_t pathAddr, uintptr_t argsAddr, uintptr_t envpAddr,
					 Elf32_HeaderVerifyCallback elfVerifCallback, uintptr_t *userentry, uintptr_t *stack);
int Syscall_Wait4(int pid, uintptr_t wstatusAddr, int options, uintptr_t rusageAddr);
/*
int Syscall_GetDirectoryEntries(struct CPUState *state);
int Syscall_Chdir(struct CPUState *state);
int Syscall_Fchdir(struct CPUState *state);
int Syscall_GetCWD(struct CPUState *state);
int Syscall_GetPID(struct CPUState *state);
int Syscall_GetPPID(struct CPUState *state);
int Syscall_Fstat(struct CPUState *state);
*/
#endif

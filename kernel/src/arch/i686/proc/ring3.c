#include <arch/i686/cpu/idt.h>
#include <arch/i686/proc/ring3.h>
#include <arch/i686/proc/syscalls.h>

#define I686_SYSCALL_TABLE_SIZE 512

uint32_t i686_Ring3_SyscallTable[I686_SYSCALL_TABLE_SIZE] = {0};
uint32_t i686_Ring3_SyscallTableSize;

extern void i686_Ring3_SyscallEntry();

void i686_Ring3_SyscallInit() {
	i686_Ring3_SyscallTableSize = I686_SYSCALL_TABLE_SIZE;
	i686_IDT_InstallHandler(0x80, (uint32_t)i686_Ring3_SyscallEntry, I686_32BIT_TRAP_GATE, 3, 0x19);
	i686_Ring3_SyscallTable[1] = (uint32_t)i686_Syscall_Exit;
	i686_Ring3_SyscallTable[2] = (uint32_t)i686_Syscall_Fork;
	i686_Ring3_SyscallTable[3] = (uint32_t)i686_Syscall_Read;
	i686_Ring3_SyscallTable[4] = (uint32_t)i686_Syscall_Write;
	i686_Ring3_SyscallTable[5] = (uint32_t)i686_Syscall_Open;
	i686_Ring3_SyscallTable[6] = (uint32_t)i686_Syscall_Close;
	i686_Ring3_SyscallTable[11] = (uint32_t)i686_Syscall_Wait4;
	i686_Ring3_SyscallTable[12] = (uint32_t)i686_Syscall_Chdir;
	i686_Ring3_SyscallTable[13] = (uint32_t)i686_Syscall_Fchdir;
	i686_Ring3_SyscallTable[20] = (uint32_t)i686_Syscall_GetPID;
	i686_Ring3_SyscallTable[39] = (uint32_t)i686_Syscall_GetPPID;
	i686_Ring3_SyscallTable[53] = (uint32_t)i686_Syscall_Fstat;
	i686_Ring3_SyscallTable[59] = (uint32_t)i686_Syscall_Execve;
	i686_Ring3_SyscallTable[67] = (uint32_t)i686_Syscall_GetTimeOfDay;
	i686_Ring3_SyscallTable[73] = (uint32_t)i686_Syscall_MemoryUnmap;
	i686_Ring3_SyscallTable[99] = (uint32_t)i686_Syscall_GetDirectoryEntries;
	i686_Ring3_SyscallTable[197] = (uint32_t)i686_Syscall_MemoryMap;
	i686_Ring3_SyscallTable[304] = (uint32_t)i686_Syscall_GetCWD;
}

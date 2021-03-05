#include <arch/i686/proc/elf32.h>
#include <arch/i686/proc/ring3.h>
#include <arch/i686/proc/syscalls.h>
#include <common/core/fd/cwd.h>
#include <common/core/fd/fdtable.h>
#include <common/core/fd/vfs.h>
#include <common/core/memory/heap.h>
#include <common/core/memory/msecurity.h>
#include <common/core/memory/virt.h>
#include <common/core/proc/abis.h>
#include <common/core/proc/elf32.h>
#include <common/core/proc/mutex.h>
#include <common/core/proc/proc.h>
#include <common/core/proc/proclayout.h>
#include <common/core/proc/syscalls.h>
#include <common/lib/kmsg.h>
#include <hal/proc/extended.h>

#define MAX_PATH_LEN 65536
#define MAX_IO_BUF_LEN 65536
#define MAX_ARGS 1024
#define MAX_ENVP 1024
#define MAX_ARGS_LEN 65536
#define PROCESS_STACK_SIZE 0x100000

void i686_Syscall_Exit(struct i686_CPUState *state) {
	uint32_t statusStart = state->esp + 4;
	uint32_t statusEnd = state->esp + 8;
	struct VirtualMM_AddressSpace *space = VirtualMM_GetCurrentAddressSpace();
	Mutex_Lock(&(space->mutex));
	if (!MemorySecurity_VerifyMemoryRangePermissions(statusStart, statusEnd, MSECURITY_UR)) {
		Mutex_Unlock(&(space->mutex));
		state->eax = -1;
		return;
	}
	int status = *(int *)statusStart;
	Mutex_Unlock(&(space->mutex));
	Proc_Exit(status);
	KernelLog_ErrorMsg("i686 ExitProcess System Call", "Failed to terminate process");
}

void i686_Syscall_Open(struct i686_CPUState *state) {
	uint32_t paramsStart = state->esp + 4;
	uint32_t paramsEnd = state->esp + 12;
	struct VirtualMM_AddressSpace *space = VirtualMM_GetCurrentAddressSpace();
	Mutex_Lock(&(space->mutex));
	if (!MemorySecurity_VerifyMemoryRangePermissions(paramsStart, paramsEnd, MSECURITY_UR)) {
		Mutex_Unlock(&(space->mutex));
		state->eax = -1;
		return;
	}
	uint32_t pathAddr = (*(uint32_t *)paramsStart);
	int perms = *(int *)(paramsStart + 4);
	Mutex_Unlock(&(space->mutex));
	state->eax = Syscall_Open(pathAddr, perms);
}

void i686_Syscall_Read(struct i686_CPUState *state) {
	uint32_t paramsStart = state->esp + 4;
	uint32_t paramsEnd = state->esp + 16;
	struct VirtualMM_AddressSpace *space = VirtualMM_GetCurrentAddressSpace();
	Mutex_Lock(&(space->mutex));
	if (!MemorySecurity_VerifyMemoryRangePermissions(paramsStart, paramsEnd, MSECURITY_UR)) {
		Mutex_Unlock(&(space->mutex));
		state->eax = -1;
		return;
	}
	int fd = *(int *)(paramsStart);
	uint32_t bufferAddr = *(uint32_t *)(paramsStart + 4);
	int size = *(int *)(paramsStart + 8);
	Mutex_Unlock(&(space->mutex));
	state->eax = Syscall_Read(fd, bufferAddr, size);
}

void i686_Syscall_Write(struct i686_CPUState *state) {
	uint32_t paramsStart = state->esp + 4;
	uint32_t paramsEnd = state->esp + 16;
	struct VirtualMM_AddressSpace *space = VirtualMM_GetCurrentAddressSpace();
	Mutex_Lock(&(space->mutex));
	if (!MemorySecurity_VerifyMemoryRangePermissions(paramsStart, paramsEnd, MSECURITY_UR)) {
		Mutex_Unlock(&(space->mutex));
		state->eax = -1;
		return;
	}
	int fd = *(int *)(paramsStart);
	uint32_t bufferAddr = *(uint32_t *)(paramsStart + 4);
	int size = *(int *)(paramsStart + 8);
	Mutex_Unlock(&(space->mutex));
	state->eax = Syscall_Write(fd, bufferAddr, size);
}

void i686_Syscall_Close(struct i686_CPUState *state) {
	uint32_t paramsStart = state->esp + 4;
	uint32_t paramsEnd = state->esp + 8;
	struct VirtualMM_AddressSpace *space = VirtualMM_GetCurrentAddressSpace();
	Mutex_Lock(&(space->mutex));
	if (!MemorySecurity_VerifyMemoryRangePermissions(paramsStart, paramsEnd, MSECURITY_UR)) {
		Mutex_Unlock(&(space->mutex));
		state->eax = -1;
		return;
	}
	int fd = *(int *)(paramsStart);
	Mutex_Unlock(&(space->mutex));
	state->eax = FileTable_FileClose(NULL, fd);
}

void i686_Syscall_MemoryUnmap(struct i686_CPUState *state) {
	uint32_t paramsStart = state->esp + 4;
	uint32_t paramsEnd = state->esp + 12;
	struct VirtualMM_AddressSpace *space = VirtualMM_GetCurrentAddressSpace();
	Mutex_Lock(&(space->mutex));
	if (!MemorySecurity_VerifyMemoryRangePermissions(paramsStart, paramsEnd, MSECURITY_UR)) {
		Mutex_Unlock(&(space->mutex));
		state->eax = -1;
		return;
	}
	uintptr_t addr = *(uintptr_t *)paramsStart;
	size_t size = *(size_t *)(paramsStart + 4);
	Mutex_Unlock(&(space->mutex));
	state->eax = Syscall_MemoryUnmap(addr, size);
}

void i686_Syscall_MemoryMap(struct i686_CPUState *state) {
	uint32_t paramsStart = state->esp + 4;
	uint32_t paramsEnd = state->esp + 28;
	struct VirtualMM_AddressSpace *space = VirtualMM_GetCurrentAddressSpace();
	Mutex_Lock(&(space->mutex));
	if (!MemorySecurity_VerifyMemoryRangePermissions(paramsStart, paramsEnd, MSECURITY_UR)) {
		Mutex_Unlock(&(space->mutex));
		state->eax = -1;
		return;
	}
	uintptr_t addr = *(uintptr_t *)paramsStart;
	size_t size = *(size_t *)(paramsStart + 4);
	int prot = *(int *)(paramsStart + 8);
	int flags = *(int *)(paramsStart + 12);
	Mutex_Unlock(&(space->mutex));
	state->eax = Syscall_MemoryMap(addr, size, prot, flags);
}

void i686_Syscall_Fork(struct i686_CPUState *state) {
	struct Proc_ProcessID id = Syscall_Fork((void *)state);
	if (!Proc_IsValidProcessID(id)) {
		state->eax = -1;
		return;
	}
	state->eax = id.id;
	struct Proc_Process *newProcess = Proc_GetProcessData(id);
	((struct i686_CPUState *)(newProcess->processState))->eax = 0;
	Proc_Resume(id);
}

void i686_Syscall_Execve(struct i686_CPUState *state) {
	uint32_t paramsStart = state->esp + 4;
	uint32_t paramsEnd = state->esp + 16;
	struct VirtualMM_AddressSpace *space = VirtualMM_GetCurrentAddressSpace();
	Mutex_Lock(&(space->mutex));
	if (!MemorySecurity_VerifyMemoryRangePermissions(paramsStart, paramsEnd, MSECURITY_UR)) {
		Mutex_Unlock(&(space->mutex));
		state->eax = -1;
		return;
	}
	uint32_t pathAddr = *(uint32_t *)(paramsStart);
	uint32_t argsAddr = *(uint32_t *)(paramsStart + 4);
	uint32_t envpAddr = *(uint32_t *)(paramsStart + 8);
	Mutex_Unlock(&(space->mutex));

	uintptr_t entrypoint, stack;
	int result = Syscall_Execve32(pathAddr, argsAddr, envpAddr, i686_Elf32_HeaderVerifyCallback, &entrypoint, &stack);

	if (result == 0) {
		i686_Ring3_Switch(entrypoint, stack);
	} else {
		state->eax = result;
	}
}

void i686_Syscall_Wait4(struct i686_CPUState *state) {
	uint32_t paramsStart = state->esp + 4;
	uint32_t paramsEnd = state->esp + 20;
	struct VirtualMM_AddressSpace *space = VirtualMM_GetCurrentAddressSpace();
	Mutex_Lock(&(space->mutex));
	if (!MemorySecurity_VerifyMemoryRangePermissions(paramsStart, paramsEnd, MSECURITY_UR)) {
		Mutex_Unlock(&(space->mutex));
		state->eax = -1;
		return;
	}
	int pid = *(int *)(paramsStart);
	uint32_t wstatusAddr = *(uint32_t *)(paramsStart + 4);
	int options = *(int *)(paramsStart + 8);
	uint32_t rusageAddr = *(uint32_t *)(paramsStart + 12);
	Mutex_Unlock(&(space->mutex));
	state->eax = Syscall_Wait4(pid, wstatusAddr, options, rusageAddr);
}

void i686_Syscall_GetDirectoryEntries(struct i686_CPUState *state) {
	uint32_t paramsStart = state->esp + 4;
	uint32_t paramsEnd = state->esp + 16;
	struct VirtualMM_AddressSpace *space = VirtualMM_GetCurrentAddressSpace();
	Mutex_Lock(&(space->mutex));
	if (!MemorySecurity_VerifyMemoryRangePermissions(paramsStart, paramsEnd, MSECURITY_UR)) {
		Mutex_Unlock(&(space->mutex));
		state->eax = -1;
		return;
	}
	int fd = *(int *)(paramsStart);
	struct DirectoryEntry *entries = *(struct DirectoryEntry **)(paramsStart + 4);
	int bufLength = *(int *)(paramsStart + 8);
	if (bufLength < 0 || bufLength > (int)(MAX_IO_BUF_LEN / sizeof(struct DirectoryEntry))) {
		state->eax = -1;
		return;
	}
	struct DirectoryEntry *entriesEnd = entries + bufLength;
	Mutex_Unlock(&(space->mutex));
	struct DirectoryEntry *inKernelCopy = Heap_AllocateMemory(bufLength * sizeof(struct DirectoryEntry));
	if (inKernelCopy == NULL) {
		state->eax = -1;
		return;
	}
	memset(inKernelCopy, 0, bufLength * sizeof(struct DirectoryEntry));
	int result = FileTable_FileReaddir(NULL, fd, inKernelCopy, bufLength);
	Mutex_Lock(&(space->mutex));
	if (!MemorySecurity_VerifyMemoryRangePermissions((uintptr_t)entries, (uintptr_t)entriesEnd, MSECURITY_UW)) {
		Mutex_Unlock(&(space->mutex));
		Heap_FreeMemory(inKernelCopy, bufLength * sizeof(struct DirectoryEntry));
		state->eax = -1;
		return;
	}
	if (result > 0) {
		memcpy(entries, inKernelCopy, result * sizeof(struct DirectoryEntry));
	}
	Mutex_Unlock(&(space->mutex));
	state->eax = result;
}

void i686_Syscall_Chdir(struct i686_CPUState *state) {
	struct Proc_Process *thisProcess = Proc_GetProcessData(Proc_GetProcessID());
	uint32_t paramsStart = state->esp + 4;
	uint32_t paramsEnd = state->esp + 8;
	struct VirtualMM_AddressSpace *space = VirtualMM_GetCurrentAddressSpace();
	Mutex_Lock(&(space->mutex));
	if (!MemorySecurity_VerifyMemoryRangePermissions(paramsStart, paramsEnd, MSECURITY_UR)) {
		Mutex_Unlock(&(space->mutex));
		state->eax = -1;
		return;
	}
	uintptr_t pathAddr = *(uintptr_t *)(paramsStart);
	int pathLen = MemorySecurity_VerifyCString(pathAddr, MAX_PATH_LEN, MSECURITY_UR);
	if (pathLen == -1) {
		Mutex_Unlock(&(space->mutex));
		state->eax = -1;
		return;
	}
	struct File *newWorkingDirectory = VFS_OpenAt(thisProcess->cwd, (char *)pathAddr, VFS_O_RDONLY);
	if (newWorkingDirectory == NULL) {
		Mutex_Unlock(&(space->mutex));
		state->eax = -1;
		return;
	}
	if (newWorkingDirectory->dentry->cwd == NULL) {
		File_Drop(newWorkingDirectory);
		Mutex_Unlock(&(space->mutex));
		state->eax = -1;
		return;
	}
	Mutex_Unlock(&(space->mutex));
	File_Drop(thisProcess->cwd);
	thisProcess->cwd = newWorkingDirectory;
	state->eax = 0;
	return;
}

void i686_Syscall_Fchdir(struct i686_CPUState *state) {
	struct Proc_Process *thisProcess = Proc_GetProcessData(Proc_GetProcessID());
	uint32_t paramsStart = state->esp + 4;
	uint32_t paramsEnd = state->esp + 8;
	struct VirtualMM_AddressSpace *space = VirtualMM_GetCurrentAddressSpace();
	Mutex_Lock(&(space->mutex));
	if (!MemorySecurity_VerifyMemoryRangePermissions(paramsStart, paramsEnd, MSECURITY_UR)) {
		Mutex_Unlock(&(space->mutex));
		state->eax = -1;
		return;
	}
	int fd = *(int *)(paramsStart);
	Mutex_Unlock(&(space->mutex));
	struct File *file = FileTable_Grab(NULL, fd);
	if (file == NULL) {
		Mutex_Unlock(&(space->mutex));
		state->eax = -1;
		return;
	}
	if (file->dentry->cwd == NULL) {
		File_Drop(file);
		Mutex_Unlock(&(space->mutex));
		state->eax = -1;
		return;
	}
	File_Drop(thisProcess->cwd);
	thisProcess->cwd = file;
	state->eax = 0;
	return;
}

void i686_Syscall_GetCWD(struct i686_CPUState *state) {
	uint32_t paramsStart = state->esp + 4;
	uint32_t paramsEnd = state->esp + 12;
	struct VirtualMM_AddressSpace *space = VirtualMM_GetCurrentAddressSpace();
	Mutex_Lock(&(space->mutex));
	if (!MemorySecurity_VerifyMemoryRangePermissions(paramsStart, paramsEnd, MSECURITY_UR)) {
		Mutex_Unlock(&(space->mutex));
		state->eax = -1;
		return;
	}
	uintptr_t bufferAddr = *(uintptr_t *)(paramsStart);
	size_t size = *(size_t *)(paramsStart + 4);
	if (!MemorySecurity_VerifyMemoryRangePermissions(bufferAddr, bufferAddr + size, MSECURITY_UW)) {
		Mutex_Unlock(&(space->mutex));
		state->eax = -1;
		return;
	}
	struct Proc_Process *thisProcess = Proc_GetProcessData(Proc_GetProcessID());
	struct File *cwd = thisProcess->cwd;
	int result = CWD_GetWorkingDirectoryPath(cwd->dentry->cwd, (char *)bufferAddr, size);
	Mutex_Unlock(&(space->mutex));
	state->eax = result;
}

void i686_Syscall_GetPID(struct i686_CPUState *state) {
	struct Proc_Process *thisProcess = Proc_GetProcessData(Proc_GetProcessID());
	state->eax = thisProcess->pid.id;
}

void i686_Syscall_GetPPID(struct i686_CPUState *state) {
	struct Proc_Process *thisProcess = Proc_GetProcessData(Proc_GetProcessID());
	state->eax = thisProcess->ppid.id;
}

void i686_Syscall_Fstat(struct i686_CPUState *state) {
	uint32_t paramsStart = state->esp + 4;
	uint32_t paramsEnd = state->esp + 12;
	struct VirtualMM_AddressSpace *space = VirtualMM_GetCurrentAddressSpace();
	Mutex_Lock(&(space->mutex));
	if (!MemorySecurity_VerifyMemoryRangePermissions(paramsStart, paramsEnd, MSECURITY_UR)) {
		Mutex_Unlock(&(space->mutex));
		state->eax = -1;
		return;
	}
	int fd = *(int *)(paramsStart);
	uintptr_t statAddr = *(uintptr_t *)(paramsStart + 4);
	if (!MemorySecurity_VerifyMemoryRangePermissions(statAddr, statAddr + sizeof(struct VFS_Stat), MSECURITY_UW)) {
		Mutex_Unlock(&(space->mutex));
		state->eax = -1;
		return;
	}
	struct VFS_Stat *stat = (struct VFS_Stat *)statAddr;
	state->eax = FileTable_FileStat(NULL, fd, stat);
	Mutex_Unlock(&(space->mutex));
}

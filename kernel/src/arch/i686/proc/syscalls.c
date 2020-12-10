#include <arch/i686/proc/syscalls.h>
#include <common/core/fd/fdtable.h>
#include <common/core/fd/vfs.h>
#include <common/core/memory/heap.h>
#include <common/core/memory/msecurity.h>
#include <common/core/memory/virt.h>
#include <common/core/proc/abis.h>
#include <common/core/proc/mutex.h>
#include <common/core/proc/proc.h>
#include <common/core/proc/proclayout.h>
#include <common/lib/kmsg.h>

#define MAX_PATH_LEN 65536
#define MAX_IO_BUF_LEN 65536

// NOTE: Darwin system call API dictates that stack is 16 byte aligned on system call

void i686_Syscall_Exit(struct i686_CPUState *state) {
	uint32_t statusStart = state->esp + 12;
	uint32_t statusEnd = state->esp + 16;
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
	while (true) {
		asm volatile("nop");
	}
}

void i686_Syscall_Open(struct i686_CPUState *state) {
	uint32_t paramsStart = state->esp + 8;
	uint32_t paramsEnd = state->esp + 16;
	struct VirtualMM_AddressSpace *space = VirtualMM_GetCurrentAddressSpace();
	Mutex_Lock(&(space->mutex));
	if (!MemorySecurity_VerifyMemoryRangePermissions(paramsStart, paramsEnd, MSECURITY_UR)) {
		Mutex_Unlock(&(space->mutex));
		state->eax = -1;
		return;
	}
	uint32_t pathAddr = (*(uint32_t *)paramsStart);
	int perms = *(int *)(paramsStart + 4);
	int pathLen = MemorySecurity_VerifyCString(pathAddr, MAX_PATH_LEN, MSECURITY_UR);
	if (pathLen < -1) {
	}
	char *pathCopy = Heap_AllocateMemory(pathLen + 1);
	if (pathCopy == NULL) {
		Mutex_Unlock(&(space->mutex));
		state->eax = -1;
		return;
	}
	memcpy(pathCopy, (void *)pathAddr, pathLen);
	pathCopy[pathLen] = '\0';
	Mutex_Unlock(&(space->mutex));
	struct File *file = VFS_Open(pathCopy, perms);
	if (file == NULL) {
		Heap_FreeMemory(pathCopy, pathLen + 1);
		state->eax = -1;
		return;
	}
	Heap_FreeMemory(pathCopy, pathLen + 1);
	int result = FileTable_AllocateFileSlot(NULL, file);
	if (result == -1) {
		File_Drop(file);
		state->eax = -1;
		return;
	}
	state->eax = result;
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
	if (size < 0 || size > MAX_IO_BUF_LEN) {
		Mutex_Unlock(&(space->mutex));
		state->eax = -1;
		return;
	}
	Mutex_Unlock(&(space->mutex));
	char *buf = Heap_AllocateMemory(size);
	if (buf == NULL) {
		state->eax = -1;
		return;
	}
	int result = FileTable_FileRead(NULL, fd, buf, size);
	Mutex_Lock(&(space->mutex));
	if (!MemorySecurity_VerifyMemoryRangePermissions(bufferAddr, bufferAddr + size, MSECURITY_UW)) {
		Heap_FreeMemory(buf, size);
		Mutex_Unlock(&(space->mutex));
		state->eax = -1;
		return;
	}
	if (result > 0) {
		memcpy((void *)bufferAddr, buf, result);
	}
	Heap_FreeMemory(buf, size);
	Mutex_Unlock(&(space->mutex));
	state->eax = result;
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
	if (size < 0 || size > MAX_IO_BUF_LEN) {
		Mutex_Unlock(&(space->mutex));
		state->eax = -1;
		return;
	}
	char *buf = Heap_AllocateMemory(size);
	if (buf == NULL) {
		Mutex_Unlock(&(space->mutex));
		state->eax = -1;
		return;
	}
	if (!MemorySecurity_VerifyMemoryRangePermissions(bufferAddr, bufferAddr + size, MSECURITY_UR)) {
		Heap_FreeMemory(buf, size);
		Mutex_Unlock(&(space->mutex));
		state->eax = -1;
		return;
	}
	memcpy(buf, (void *)bufferAddr, size);
	Mutex_Unlock(&(space->mutex));
	int result = FileTable_FileWrite(NULL, fd, buf, size);
	Heap_FreeMemory(buf, size);
	state->eax = result;
}

void i686_Syscall_Close(struct i686_CPUState *state) {
	uint32_t paramsStart = state->esp + 12;
	uint32_t paramsEnd = state->esp + 16;
	struct VirtualMM_AddressSpace *space = VirtualMM_GetCurrentAddressSpace();
	Mutex_Lock(&(space->mutex));
	if (!MemorySecurity_VerifyMemoryRangePermissions(paramsStart, paramsEnd, MSECURITY_UR)) {
		Mutex_Unlock(&(space->mutex));
		state->eax = -1;
		return;
	}
	int fd = *(int *)(paramsStart);
	Mutex_Unlock(&(space->mutex));
	int result = FileTable_FileClose(NULL, fd);
	state->eax = result;
}

void i686_Syscall_MemoryUnmap(struct i686_CPUState *state) {
	uint32_t paramsStart = state->esp + 8;
	uint32_t paramsEnd = state->esp + 16;
	struct VirtualMM_AddressSpace *space = VirtualMM_GetCurrentAddressSpace();
	Mutex_Lock(&(space->mutex));
	if (!MemorySecurity_VerifyMemoryRangePermissions(paramsStart, paramsEnd, MSECURITY_UR)) {
		Mutex_Unlock(&(space->mutex));
		state->eax = 0;
		return;
	}
	uintptr_t addr = *(uintptr_t *)paramsStart;
	size_t size = *(size_t *)(paramsStart + 4);
	if (addr % HAL_VirtualMM_PageSize != 0) {
		Mutex_Unlock(&(space->mutex));
		state->eax = 0;
		return;
	}
	if (size % HAL_VirtualMM_PageSize != 0) {
		Mutex_Unlock(&(space->mutex));
		state->eax = 0;
		return;
	}
	int result = VirtualMM_MemoryUnmap(NULL, addr, size, false);
	Mutex_Unlock(&(space->mutex));
	state->eax = result;
	return;
}

void i686_Syscall_MemoryMap(struct i686_CPUState *state) {
	uint32_t paramsStart = state->esp + 8;
	uint32_t paramsEnd = state->esp + 32;
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

	if ((prot & ~PROT_MASK) != 0) {
		Mutex_Unlock(&(space->mutex));
		state->eax = -1;
		return;
	}
	if ((flags & MAP_ANON) == 0) {
		Mutex_Unlock(&(space->mutex));
		state->eax = -1;
		return;
	}
	if (addr % HAL_VirtualMM_PageSize != 0) {
		Mutex_Unlock(&(space->mutex));
		state->eax = -1;
		return;
	}
	if (size % HAL_VirtualMM_PageSize != 0) {
		Mutex_Unlock(&(space->mutex));
		state->eax = -1;
		return;
	}

	int halFlags = HAL_VIRT_FLAGS_USER_ACCESSIBLE;
	if ((prot & PROT_READ) != 0) {
		halFlags |= HAL_VIRT_FLAGS_READABLE;
	}
	if ((prot & PROT_WRITE) != 0) {
		halFlags |= HAL_VIRT_FLAGS_WRITABLE;
	}
	if ((prot & PROT_EXEC) != 0) {
		halFlags |= HAL_VIRT_FLAGS_EXECUTABLE;
	}

	struct VirtualMM_MemoryRegionNode *region = VirtualMM_MemoryMap(NULL, addr, size, HAL_VIRT_FLAGS_WRITABLE, false);
	if (region == NULL) {
		Mutex_Unlock(&(space->mutex));
		state->eax = 0;
		return;
	}
	memset((void *)(region->base.start), 0, region->base.size);
	VirtualMM_MemoryRetype(NULL, region, halFlags);
	Mutex_Unlock(&(space->mutex));

	state->eax = region->base.start;
}

void i686_Syscall_Fork(struct i686_CPUState *state) {
	struct Proc_ProcessID newProcess = Proc_MakeNewProcess(Proc_GetProcessID());
	if (!Proc_IsValidProcessID(newProcess)) {
		state->eax = -1;
		return;
	}
	struct Proc_Process *newProcessData = Proc_GetProcessData(newProcess);
	newProcessData->fdTable = FileTable_Fork(NULL);
	if (newProcessData->fdTable == NULL) {
		Proc_Dispose(newProcessData);
		state->eax = -1;
		return;
	}
	newProcessData->addressSpace = VirtualMM_CopyCurrentAddressSpace();
	if (newProcessData->addressSpace == NULL) {
		FileTable_Drop(newProcessData->fdTable);
		Proc_Dispose(newProcessData);
		state->eax = -1;
		return;
	}
	memcpy(newProcessData->processState, state, sizeof(struct i686_CPUState));
	((struct i686_CPUState *)newProcessData->processState)->eax = 0;
	state->eax = newProcessData->pid.id;
	Proc_Resume(newProcess);
}

void i686_Syscall_Execve(struct i686_CPUState *state) {
	state->eax = -1;
}

void i686_Syscall_Wait4(struct i686_CPUState *state) {
	state->eax = -1;
}
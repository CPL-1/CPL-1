#include <arch/i686/proc/elf32.h>
#include <arch/i686/proc/ring3.h>
#include <arch/i686/proc/syscalls.h>
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
#include <common/lib/kmsg.h>

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
	if (size < 0) {
		Mutex_Unlock(&(space->mutex));
		state->eax = -1;
		return;
	}
	if (size > MAX_IO_BUF_LEN) {
		size = MAX_IO_BUF_LEN;
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
	if (size < 0) {
		Mutex_Unlock(&(space->mutex));
		state->eax = -1;
		return;
	}
	if (size > MAX_IO_BUF_LEN) {
		size = MAX_IO_BUF_LEN;
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
	int result = FileTable_FileClose(NULL, fd);
	state->eax = result;
}

void i686_Syscall_MemoryUnmap(struct i686_CPUState *state) {
	uint32_t paramsStart = state->esp + 4;
	uint32_t paramsEnd = state->esp + 12;
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
	if ((prot & 1) != 0) {
		halFlags |= HAL_VIRT_FLAGS_READABLE;
	}
	if ((prot & 2) != 0) {
		halFlags |= HAL_VIRT_FLAGS_WRITABLE;
	}
	if ((prot & 4) != 0) {
		halFlags |= HAL_VIRT_FLAGS_EXECUTABLE;
	}

	struct VirtualMM_MemoryRegionNode *region = VirtualMM_MemoryMap(NULL, addr, size, HAL_VIRT_FLAGS_WRITABLE, false);
	if (region == NULL) {
		Mutex_Unlock(&(space->mutex));
		state->eax = -1;
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

static void i686_Syscall_ExecveCleanupArgs(char *pathCopy, char **argsCopy, char **envpCopy) {
	int pathLength = strlen(pathCopy);
	Heap_FreeMemory((void *)pathCopy, pathLength);
	int argsSize = 0;
	int envpSize = 0;
	for (int i = 0; argsCopy[i] != NULL; ++i) {
		++argsSize;
		char *argCopy = argsCopy[i];
		if (argCopy != NULL) {
			Heap_FreeMemory((void *)argCopy, strlen(argCopy) + 1);
		}
	}
	for (int i = 0; envpCopy[i] != NULL; ++i) {
		++envpSize;
		char *envCopy = envpCopy[i];
		if (envCopy != NULL) {
			Heap_FreeMemory((void *)envCopy, strlen(envCopy) + 1);
		}
	}
	Heap_FreeMemory((void *)argsCopy, (argsSize + 1) * 4);
	Heap_FreeMemory((void *)envpCopy, (envpSize + 1) * 4);
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
	int pathLength = MemorySecurity_VerifyCString(pathAddr, MAX_PATH_LEN, MSECURITY_UR);
	int argsCount = MemorySecurity_VerifyNullTerminatedPointerList(argsAddr, MAX_ARGS, MSECURITY_UR);
	int envsCount = MemorySecurity_VerifyNullTerminatedPointerList(envpAddr, MAX_ENVP, MSECURITY_UR);
	if (argsCount == -1 || envsCount == -1 || pathLength == -1) {
		Mutex_Unlock(&(space->mutex));
		state->eax = -1;
		return;
	}
	int fullArgsLength = 0;
	for (int i = 0; i < argsCount; ++i) {
		int argLength = MemorySecurity_VerifyCString(*(uint32_t *)(argsAddr + 4 * i), MAX_ARGS_LEN, MSECURITY_UR);
		if (argLength == -1) {
			Mutex_Unlock(&(space->mutex));
			state->eax = -1;
			return;
		}
		fullArgsLength += argLength + 1;
	}
	int fullEnvpLength = 0;
	for (int i = 0; i < envsCount; ++i) {
		int envLength = MemorySecurity_VerifyCString(*(uint32_t *)(envpAddr + 4 * i), MAX_ARGS_LEN, MSECURITY_UR);
		if (envLength == -1) {
			Mutex_Unlock(&(space->mutex));
			state->eax = -1;
			return;
		}
		fullEnvpLength += envLength + 1;
	}

	char *pathCopy = Heap_AllocateMemory(pathLength + 1);
	if (pathCopy == NULL) {
		Mutex_Unlock(&(space->mutex));
		state->eax = -1;
		return;
	}
	memcpy(pathCopy, (void *)pathAddr, pathLength);
	pathCopy[pathLength] = '\0';

	char **argsUser = (char **)argsAddr;
	char **envpUser = (char **)envpAddr;

	char **argsKernelCopy = Heap_AllocateMemory((argsCount + 1) * 4);
	if (argsKernelCopy == NULL) {
		Heap_FreeMemory(pathCopy, pathLength + 1);
		Mutex_Unlock(&(space->mutex));
		state->eax = -1;
		return;
	}
	char **envpKernelCopy = Heap_AllocateMemory((envsCount + 1) * 4);
	if (envpKernelCopy == NULL) {
		Heap_FreeMemory(pathCopy, pathLength + 1);
		Heap_FreeMemory(argsKernelCopy, (argsCount + 1) * 4);
		Mutex_Unlock(&(space->mutex));
		state->eax = -1;
		return;
	}
	memset(argsKernelCopy, 0, (argsCount + 1) * 4);
	memset(envpKernelCopy, 0, (envsCount + 1) * 4);

	for (int i = 0; i < argsCount; ++i) {
		char *argUser = argsUser[i];
		int len = strlen(argUser);
		char *argKernelCopy = Heap_AllocateMemory(len + 1);
		if (argKernelCopy == NULL) {
			Mutex_Unlock(&(space->mutex));
			i686_Syscall_ExecveCleanupArgs(pathCopy, argsKernelCopy, envpKernelCopy);
			state->eax = -1;
			return;
		}
		memcpy(argKernelCopy, argUser, len);
		argsKernelCopy[i] = argKernelCopy;
		argKernelCopy[len] = '\0';
	}

	for (int i = 0; i < envsCount; ++i) {
		char *envUser = envpUser[i];
		int len = strlen(envUser);
		char *envKernelCopy = Heap_AllocateMemory(len + 1);
		if (envKernelCopy == NULL) {
			Mutex_Unlock(&(space->mutex));
			i686_Syscall_ExecveCleanupArgs(pathCopy, argsKernelCopy, envpKernelCopy);
			state->eax = -1;
			return;
		}
		memcpy(envKernelCopy, envUser, len + 1);
		envpKernelCopy[i] = envKernelCopy;
		envKernelCopy[len] = '\0';
	}

	Mutex_Unlock(&(space->mutex));

	struct VirtualMM_AddressSpace *newSpace = VirtualMM_MakeNewAddressSpace();
	if (space == NULL) {
		i686_Syscall_ExecveCleanupArgs(pathCopy, argsKernelCopy, envpKernelCopy);
		state->eax = -1;
		return;
	}
	VirtualMM_SwitchToAddressSpace(newSpace);

	struct FileTable *table = FileTable_Fork(NULL);
	if (table == NULL) {
		VirtualMM_SwitchToAddressSpace(space);
		VirtualMM_DropAddressSpace(newSpace);
		i686_Syscall_ExecveCleanupArgs(pathCopy, argsKernelCopy, envpKernelCopy);
		state->eax = -1;
		return;
	}

	// We put all argument and environment variables above init process stack
	// the resulting layout will be
	// [ unmapped page at NULL ] [ init process stack ] [ arg pointers ] [ env pointers ] [NULL fake mela pointer] [
	// strings for args and envs]
	int requiredMemorySize =
		PROCESS_STACK_SIZE + (4 * (argsCount + 1)) + (4 * (envsCount + 1)) + 4 + fullArgsLength + fullEnvpLength;
	struct VirtualMM_MemoryRegionNode *node =
		VirtualMM_MemoryMap(NULL, 0, ALIGN_UP(requiredMemorySize, HAL_VirtualMM_PageSize),
							HAL_VIRT_FLAGS_WRITABLE | HAL_VIRT_FLAGS_READABLE | HAL_VIRT_FLAGS_USER_ACCESSIBLE, true);
	if (node == NULL) {
		VirtualMM_SwitchToAddressSpace(space);
		FileTable_Drop(table);
		VirtualMM_DropAddressSpace(newSpace);
		i686_Syscall_ExecveCleanupArgs(pathCopy, argsKernelCopy, envpKernelCopy);
		state->eax = -1;
		return;
	}
	memset((void *)(node->base.start), 0, ALIGN_UP(requiredMemorySize, HAL_VirtualMM_PageSize));

	// Allocate space for arguments table
	int areaOffset = PROCESS_STACK_SIZE;
	int argsOffset = areaOffset;
	areaOffset += 4 * (argsCount + 1);
	// Allocate space for environment variables table
	int envpOffset = areaOffset;
	memcpy((void *)(node->base.start + areaOffset), envpKernelCopy, 4 * (envsCount + 1));
	areaOffset += 4 * (envsCount + 1);

	char **newArgsUser = (char **)(node->base.start + argsOffset);
	char **newEnvpUser = (char **)(node->base.start + envpOffset);

	for (int i = 0; i < argsCount; ++i) {
		char *arg = argsKernelCopy[i];
		int argLen = strlen(arg);
		memcpy((void *)(node->base.start + areaOffset), arg, argLen + 1);
		newArgsUser[i] = (char *)(node->base.start + areaOffset);
		areaOffset += argLen + 1;
	}

	for (int i = 0; i < envsCount; ++i) {
		char *env = envpKernelCopy[i];
		int envLength = strlen(env);
		memcpy((void *)(node->base.start + areaOffset), env, envLength + 1);
		newEnvpUser[i] = (char *)(node->base.start + envpOffset);
		areaOffset += envLength + 1;
	}

	struct File *file = VFS_Open(pathCopy, VFS_O_RDONLY);
	i686_Syscall_ExecveCleanupArgs(pathCopy, argsKernelCopy, envpKernelCopy);
	if (file == NULL) {
		VirtualMM_SwitchToAddressSpace(space);
		FileTable_Drop(table);
		VirtualMM_DropAddressSpace(newSpace);
		state->eax = -1;
		return;
	}

	struct Elf32 *elf = Elf32_Parse(file, i686_Elf32_HeaderVerifyCallback);
	if (elf == NULL) {
		File_Drop(file);
		VirtualMM_SwitchToAddressSpace(space);
		FileTable_Drop(table);
		VirtualMM_DropAddressSpace(newSpace);
		state->eax = -1;
		return;
	}

	if (!Elf32_LoadProgramHeaders(file, elf)) {
		Elf32_Dispose(elf);
		File_Drop(file);
		VirtualMM_SwitchToAddressSpace(space);
		FileTable_Drop(table);
		VirtualMM_DropAddressSpace(newSpace);
		state->eax = -1;
		return;
	}

	File_Drop(file);

	struct Proc_Process *processData = Proc_GetProcessData(Proc_GetProcessID());
	struct FileTable *currentTable = processData->fdTable;
	FileTable_Drop(currentTable);
	VirtualMM_DropAddressSpace(space);
	processData->fdTable = table;
	processData->addressSpace = newSpace;

	uintptr_t entrypoint = elf->entryPoint;
	Elf32_Dispose(elf);
	// stack layout on entry
	// [main will be pushed here] esp here [argc: 4 bytes] [argv: 4 bytes] [env: 4 bytes]
	*(uint32_t *)(node->base.start + PROCESS_STACK_SIZE - 12) = argsCount;
	*(uint32_t *)(node->base.start + PROCESS_STACK_SIZE - 8) = node->base.start + argsOffset;
	*(uint32_t *)(node->base.start + PROCESS_STACK_SIZE - 4) = node->base.start + envpOffset;
	i686_Ring3_Switch(entrypoint, node->base.start + PROCESS_STACK_SIZE - 12);
}

void i686_Syscall_Wait4(struct i686_CPUState *state) {
	uint32_t paramsStart = state->esp + 4;
	uint32_t paramsEnd = state->esp + 20;
	struct Proc_Process *currentProcess = Proc_GetProcessData(Proc_GetProcessID());
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
	if (pid != -1) {
		state->eax = -1;
		return;
	}
	if (rusageAddr != 0) {
		state->eax = -1;
		return;
	}
	if ((options & WUNTRACED) != 0) {
		state->eax = -1;
		return;
	}
	if (currentProcess->childCount == 0) {
		state->eax = -1;
		return;
	}
	struct Proc_Process *childProcess = Proc_WaitForChildTermination((options & WNOHANG) != 0);
	if (childProcess == NULL) {
		state->eax = 0;
		return;
	}
	if (wstatusAddr != 0) {
		Mutex_Lock(&(space->mutex));
		if (!MemorySecurity_VerifyMemoryRangePermissions(wstatusAddr, wstatusAddr + sizeof(uintptr_t), MSECURITY_UR)) {
			Mutex_Unlock(&(space->mutex));
			Proc_InsertChildBack(childProcess);
			state->eax = -1;
			return;
		}
		*(uint32_t *)wstatusAddr = (childProcess->returnCode & 0xff) | ((int)(childProcess->terminatedNormally) << 7U);
		Mutex_Unlock(&(space->mutex));
	}
	state->eax = childProcess->pid.id;
	Proc_Dispose(childProcess);
}
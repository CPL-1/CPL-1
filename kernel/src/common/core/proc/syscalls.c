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
#include <hal/proc/state.h>

#define MAX_PATH_LEN 65536
#define MAX_IO_BUF_LEN 65536
#define MAX_ARGS 1024
#define MAX_ENVP 1024
#define MAX_ARGS_LEN 65536
#define PROCESS_STACK_SIZE 0x100000

int Syscall_Open(uintptr_t pathAddr, int perms) {
	struct Proc_Process *thisProcess = Proc_GetProcessData(Proc_GetProcessID());
	struct VirtualMM_AddressSpace *space = VirtualMM_GetCurrentAddressSpace();
	Mutex_Lock(&(space->mutex));
	int pathLen = MemorySecurity_VerifyCString(pathAddr, MAX_PATH_LEN, MSECURITY_UR);
	if (pathLen == -1) {
		return -1;
	}
	char *pathCopy = Heap_AllocateMemory(pathLen + 1);
	if (pathCopy == NULL) {
		Mutex_Unlock(&(space->mutex));
		return -1;
	}
	memcpy(pathCopy, (void *)pathAddr, pathLen);
	pathCopy[pathLen] = '\0';
	Mutex_Unlock(&(space->mutex));
	struct File *file = VFS_OpenAt(thisProcess->cwd, pathCopy, perms);
	Heap_FreeMemory(pathCopy, pathLen + 1);
	if (file == NULL) {
		Heap_FreeMemory(pathCopy, pathLen + 1);
		return -1;
	}
	int result = FileTable_AllocateFileSlot(NULL, file);
	if (result == -1) {
		File_Drop(file);
		return -1;
	}
	return result;
}

int Syscall_Read(int fd, uintptr_t bufferAddr, int size) {
	struct VirtualMM_AddressSpace *space = VirtualMM_GetCurrentAddressSpace();
	if (size < 0) {
		return -1;
	}
	if (size > MAX_IO_BUF_LEN) {
		size = MAX_IO_BUF_LEN;
	}
	char *buf = Heap_AllocateMemory(size);
	if (buf == NULL) {
		return -1;
	}
	int result = FileTable_FileRead(NULL, fd, buf, size);
	Mutex_Lock(&(space->mutex));
	if (!MemorySecurity_VerifyMemoryRangePermissions(bufferAddr, bufferAddr + size, MSECURITY_UW)) {
		Heap_FreeMemory(buf, size);
		Mutex_Unlock(&(space->mutex));
		return -1;
	}
	if (result > 0) {
		memcpy((void *)bufferAddr, buf, result);
	}
	Heap_FreeMemory(buf, size);
	Mutex_Unlock(&(space->mutex));
	return result;
}

int Syscall_Write(int fd, uintptr_t bufferAddr, int size) {
	struct VirtualMM_AddressSpace *space = VirtualMM_GetCurrentAddressSpace();
	if (size < 0) {
		Mutex_Unlock(&(space->mutex));
		return -1;
	}
	if (size > MAX_IO_BUF_LEN) {
		size = MAX_IO_BUF_LEN;
	}
	char *buf = Heap_AllocateMemory(size);
	if (buf == NULL) {
		Mutex_Unlock(&(space->mutex));
		return -1;
	}
	if (!MemorySecurity_VerifyMemoryRangePermissions(bufferAddr, bufferAddr + size, MSECURITY_UR)) {
		Heap_FreeMemory(buf, size);
		Mutex_Unlock(&(space->mutex));
		return -1;
	}
	memcpy(buf, (void *)bufferAddr, size);
	Mutex_Unlock(&(space->mutex));
	int result = FileTable_FileWrite(NULL, fd, buf, size);
	Heap_FreeMemory(buf, size);
	return result;
}

uintptr_t Syscall_MemoryMap(uintptr_t addr, size_t size, int prot, int flags) {
	if ((prot & ~PROT_MASK) != 0) {
		return (uintptr_t)-1;
	}
	if ((flags & MAP_ANON) == 0) {
		return (uintptr_t)-1;
	}
	if (addr % HAL_VirtualMM_PageSize != 0) {
		return (uintptr_t)-1;
	}
	if (size % HAL_VirtualMM_PageSize != 0) {
		return (uintptr_t)-1;
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
	struct VirtualMM_AddressSpace *space = VirtualMM_GetCurrentAddressSpace();
	Mutex_Lock(&(space->mutex));
	struct VirtualMM_MemoryRegionNode *region = VirtualMM_MemoryMap(NULL, addr, size, HAL_VIRT_FLAGS_WRITABLE, false);
	if (region == NULL) {
		Mutex_Unlock(&(space->mutex));
		return -1;
	}
	memset((void *)(region->base.start), 0, region->base.size);
	VirtualMM_MemoryRetype(NULL, region, halFlags);
	Mutex_Unlock(&(space->mutex));
	return region->base.start;
}

int Syscall_MemoryUnmap(uintptr_t addr, size_t size) {
	if (addr % HAL_VirtualMM_PageSize != 0) {
		return -1;
	}
	if (size % HAL_VirtualMM_PageSize != 0) {
		return -1;
	}
	struct VirtualMM_AddressSpace *space = VirtualMM_GetCurrentAddressSpace();
	Mutex_Lock(&(space->mutex));
	int result = VirtualMM_MemoryUnmap(NULL, addr, size, false);
	Mutex_Unlock(&(space->mutex));
	return result;
}

struct Proc_ProcessID Syscall_Fork(void *state) {
	struct Proc_Process *thisProcess = Proc_GetProcessData(Proc_GetProcessID());
	struct Proc_ProcessID newProcess = Proc_MakeNewProcess(Proc_GetProcessID());
	if (!Proc_IsValidProcessID(newProcess)) {
		return PROC_INVALID_PROC_ID;
	}
	struct Proc_Process *newProcessData = Proc_GetProcessData(newProcess);
	newProcessData->fdTable = FileTable_Fork(NULL);
	if (newProcessData->fdTable == NULL) {
		Proc_Dispose(newProcessData);
		return PROC_INVALID_PROC_ID;
	}
	File_Ref(thisProcess->cwd);
	newProcessData->cwd = thisProcess->cwd;
	newProcessData->addressSpace = VirtualMM_CopyCurrentAddressSpace();
	if (newProcessData->addressSpace == NULL) {
		FileTable_Drop(newProcessData->fdTable);
		Proc_Dispose(newProcessData);
		return PROC_INVALID_PROC_ID;
	}
	memcpy(newProcessData->processState, state, HAL_ProcessStateSize);
	HAL_ExtendedState_StoreTo(newProcessData->extendedState);
	return newProcess;
}

static void Syscall_ExecveCleanupArgs(char *pathCopy, char **argsCopy, char **envpCopy, int argsSize, int envpSize) {
	int pathLength = strlen(pathCopy);
	Heap_FreeMemory((void *)pathCopy, pathLength + 1);
	for (int i = 0; argsCopy[i] != NULL; ++i) {
		char *argCopy = argsCopy[i];
		if (argCopy != NULL) {
			Heap_FreeMemory((void *)argCopy, strlen(argCopy) + 1);
		}
	}
	for (int i = 0; envpCopy[i] != NULL; ++i) {
		char *envCopy = envpCopy[i];
		if (envCopy != NULL) {
			Heap_FreeMemory((void *)envCopy, strlen(envCopy) + 1);
		}
	}
	Heap_FreeMemory((void *)argsCopy, (argsSize + 1) * sizeof(uintptr_t));
	Heap_FreeMemory((void *)envpCopy, (envpSize + 1) * sizeof(uintptr_t));
}

int Syscall_Execve32(uintptr_t pathAddr, uintptr_t argsAddr, uintptr_t envpAddr,
					 Elf32_HeaderVerifyCallback elfVerifCallback, uintptr_t *userentry, uintptr_t *stack) {
	struct Proc_Process *thisProcess = Proc_GetProcessData(Proc_GetProcessID());
	struct VirtualMM_AddressSpace *space = VirtualMM_GetCurrentAddressSpace();
	Mutex_Lock(&(space->mutex));
	int pathLength = MemorySecurity_VerifyCString(pathAddr, MAX_PATH_LEN, MSECURITY_UR);
	int argsCount = MemorySecurity_VerifyNullTerminatedPointerList(argsAddr, MAX_ARGS, MSECURITY_UR);
	int envsCount = MemorySecurity_VerifyNullTerminatedPointerList(envpAddr, MAX_ENVP, MSECURITY_UR);
	if (argsCount == -1 || envsCount == -1 || pathLength == -1) {
		Mutex_Unlock(&(space->mutex));
		return -1;
	}
	int fullArgsLength = 0;
	for (int i = 0; i < argsCount; ++i) {
		int argLength = MemorySecurity_VerifyCString(*(uint32_t *)(argsAddr + 4 * i), MAX_ARGS_LEN, MSECURITY_UR);
		if (argLength == -1) {
			Mutex_Unlock(&(space->mutex));
			return -1;
		}
		fullArgsLength += argLength + 1;
	}
	int fullEnvpLength = 0;
	for (int i = 0; i < envsCount; ++i) {
		int envLength = MemorySecurity_VerifyCString(*(uint32_t *)(envpAddr + 4 * i), MAX_ARGS_LEN, MSECURITY_UR);
		if (envLength == -1) {
			Mutex_Unlock(&(space->mutex));
			return -1;
		}
		fullEnvpLength += envLength + 1;
	}

	char *pathCopy = Heap_AllocateMemory(pathLength + 1);
	if (pathCopy == NULL) {
		Mutex_Unlock(&(space->mutex));
		return -1;
	}
	memcpy(pathCopy, (void *)pathAddr, pathLength);
	pathCopy[pathLength] = '\0';

	char **argsUser = (char **)argsAddr;
	char **envpUser = (char **)envpAddr;

	char **argsKernelCopy = Heap_AllocateMemory((argsCount + 1) * 4);
	if (argsKernelCopy == NULL) {
		Heap_FreeMemory(pathCopy, pathLength + 1);
		Mutex_Unlock(&(space->mutex));
		return -1;
	}
	char **envpKernelCopy = Heap_AllocateMemory((envsCount + 1) * 4);
	if (envpKernelCopy == NULL) {
		Heap_FreeMemory(pathCopy, pathLength + 1);
		Heap_FreeMemory(argsKernelCopy, (argsCount + 1) * 4);
		Mutex_Unlock(&(space->mutex));
		return -1;
	}
	memset(argsKernelCopy, 0, (argsCount + 1) * 4);
	memset(envpKernelCopy, 0, (envsCount + 1) * 4);

	for (int i = 0; i < argsCount; ++i) {
		char *argUser = argsUser[i];
		int len = strlen(argUser);
		char *argKernelCopy = Heap_AllocateMemory(len + 1);
		if (argKernelCopy == NULL) {
			Mutex_Unlock(&(space->mutex));
			Syscall_ExecveCleanupArgs(pathCopy, argsKernelCopy, envpKernelCopy, argsCount, envsCount);
			return -1;
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
			Syscall_ExecveCleanupArgs(pathCopy, argsKernelCopy, envpKernelCopy, argsCount, envsCount);
			return -1;
		}
		memcpy(envKernelCopy, envUser, len + 1);
		envpKernelCopy[i] = envKernelCopy;
		envKernelCopy[len] = '\0';
	}

	Mutex_Unlock(&(space->mutex));

	struct VirtualMM_AddressSpace *newSpace = VirtualMM_MakeNewAddressSpace();
	if (space == NULL) {
		Syscall_ExecveCleanupArgs(pathCopy, argsKernelCopy, envpKernelCopy, argsCount, envsCount);
		return -1;
	}
	VirtualMM_SwitchToAddressSpace(newSpace);

	struct FileTable *table = FileTable_Fork(NULL);
	if (table == NULL) {
		VirtualMM_SwitchToAddressSpace(space);
		VirtualMM_DropAddressSpace(newSpace);
		Syscall_ExecveCleanupArgs(pathCopy, argsKernelCopy, envpKernelCopy, argsCount, envsCount);
		return -1;
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
		Syscall_ExecveCleanupArgs(pathCopy, argsKernelCopy, envpKernelCopy, argsCount, envsCount);
		return -1;
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

	struct File *file = VFS_OpenAt(thisProcess->cwd, pathCopy, VFS_O_RDONLY);
	Syscall_ExecveCleanupArgs(pathCopy, argsKernelCopy, envpKernelCopy, argsCount, envsCount);
	if (file == NULL) {
		VirtualMM_SwitchToAddressSpace(space);
		FileTable_Drop(table);
		VirtualMM_DropAddressSpace(newSpace);
		return -1;
	}

	struct Elf32 *elf = Elf32_Parse(file, elfVerifCallback);
	if (elf == NULL) {
		File_Drop(file);
		VirtualMM_SwitchToAddressSpace(space);
		FileTable_Drop(table);
		VirtualMM_DropAddressSpace(newSpace);
		return -1;
	}
	if (!Elf32_LoadProgramHeaders(file, elf)) {
		Elf32_Dispose(elf);
		File_Drop(file);
		VirtualMM_SwitchToAddressSpace(space);
		FileTable_Drop(table);
		VirtualMM_DropAddressSpace(newSpace);
		return -1;
	}
	File_Drop(file);

	struct Proc_Process *processData = Proc_GetProcessData(Proc_GetProcessID());
	struct FileTable *currentTable = processData->fdTable;
	FileTable_Drop(currentTable);
	VirtualMM_DropAddressSpace(space);
	processData->fdTable = table;
	processData->addressSpace = newSpace;

	*(uint32_t *)(node->base.start + PROCESS_STACK_SIZE - 12) = argsCount;
	*(uint32_t *)(node->base.start + PROCESS_STACK_SIZE - 8) = node->base.start + argsOffset;
	*(uint32_t *)(node->base.start + PROCESS_STACK_SIZE - 4) = node->base.start + envpOffset;
	*userentry = elf->entryPoint;
	*stack = node->base.start + PROCESS_STACK_SIZE - 12;

	Elf32_Dispose(elf);
	return 0;
}

int Syscall_Wait4(int pid, uintptr_t wstatusAddr, int options, uintptr_t rusageAddr) {
	struct Proc_Process *currentProcess = Proc_GetProcessData(Proc_GetProcessID());
	struct VirtualMM_AddressSpace *space = VirtualMM_GetCurrentAddressSpace();
	if (pid != -1) {
		return -1;
	}
	if (rusageAddr != 0) {
		return -1;
	}
	if ((options & WUNTRACED) != 0) {
		return -1;
	}
	if (currentProcess->childCount == 0) {
		return -1;
	}
	struct Proc_Process *childProcess = Proc_WaitForChildTermination((options & WNOHANG) != 0);
	if (childProcess == NULL) {
		return 0;
	}
	if (wstatusAddr != 0) {
		Mutex_Lock(&(space->mutex));
		if (!MemorySecurity_VerifyMemoryRangePermissions(wstatusAddr, wstatusAddr + sizeof(uintptr_t), MSECURITY_UR)) {
			Mutex_Unlock(&(space->mutex));
			Proc_InsertChildBack(childProcess);
			return -1;
		}
		*(uint32_t *)wstatusAddr = (childProcess->returnCode & 0xff) | ((int)(childProcess->terminatedNormally) << 7U);
		Mutex_Unlock(&(space->mutex));
	}
	int result = childProcess->pid.id;
	Proc_Dispose(childProcess);
	return result;
}

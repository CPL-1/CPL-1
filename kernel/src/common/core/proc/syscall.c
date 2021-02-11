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
#include <common/core/proc/syscall.h>
#include <common/lib/kmsg.h>
#include <hal/proc/extended.h>

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

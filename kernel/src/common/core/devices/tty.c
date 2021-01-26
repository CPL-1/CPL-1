#include <common/core/devices/tty.h>
#include <common/core/fd/fd.h>
#include <common/core/fd/fs/devfs.h>
#include <common/core/fd/vfs.h>
#include <common/core/memory/heap.h>
#include <common/core/memory/virt.h>
#include <common/core/proc/proc.h>
#include <common/lib/kmsg.h>
#include <common/lib/readline.h>
#include <common/lib/vt100.h>

#define TTY_INPUT_BUFFER_SIZE 4096

static struct Mutex TTYDevice_Mutex;

struct TTYDevice_File {
	char buf[TTY_INPUT_BUFFER_SIZE];
	int pos;
	int len;
};

int TTYDevice_Write(MAYBE_UNUSED struct File *file, int size, const char *buf) {
	if (size < 0) {
		return -1;
	}
	Mutex_Lock(&TTYDevice_Mutex);
	for (int i = 0; i < size; ++i) {
		VT100_PutCharacter(buf[i]);
	}
	VT100_Flush();
	Mutex_Unlock(&TTYDevice_Mutex);
	return size;
}

void TTYDevice_Flush() {
	Mutex_Lock(&TTYDevice_Mutex);
	VT100_Flush();
	Mutex_Unlock(&TTYDevice_Mutex);
}

int TTYDevice_Read(struct File *file, int size, char *buf) {
	struct TTYDevice_File *ttyFile = file->ctx;
	if (ttyFile->pos == ttyFile->len) {
		ttyFile->pos = 0;
		ttyFile->len = ReadLine(ttyFile->buf, TTY_INPUT_BUFFER_SIZE);
	}
	int preread = ttyFile->len - ttyFile->pos;
	if (preread > size) {
		preread = size;
	}
	memcpy(buf, ttyFile->buf + ttyFile->pos, preread);
	ttyFile->pos += preread;
	return preread;
}

void TTYDevice_Close(struct File *file) {
	FREE_OBJ(((struct TTYDevice_File *)(file->ctx)));
	VFS_FinalizeFile(file);
}

static struct FileOperations TTYDevice_FileOperations = {.read = TTYDevice_Read,
														 .write = TTYDevice_Write,
														 .readdir = NULL,
														 .lseek = NULL,
														 .flush = NULL,
														 .close = TTYDevice_Close};

struct File *TTYDevice_Open(MAYBE_UNUSED struct VFS_Inode *inode, int perm) {
	if ((perm & VFS_O_RDONLY) != 0) {
		return NULL;
	}
	struct File *ttyFile = ALLOC_OBJ(struct File);
	if (ttyFile == NULL) {
		return NULL;
	}
	struct TTYDevice_File *fileCtx = ALLOC_OBJ(struct TTYDevice_File);
	if (fileCtx == NULL) {
		FREE_OBJ(ttyFile);
		return NULL;
	}
	ttyFile->ctx = fileCtx;
	fileCtx->pos = fileCtx->len = 0;
	ttyFile->ops = &(TTYDevice_FileOperations);
	ttyFile->isATTY = true;
	return ttyFile;
}

static struct VFS_InodeOperations TTYDevice_InodeOperations = {
	.getChild = NULL,
	.open = TTYDevice_Open,
	.mkdir = NULL,
	.link = NULL,
	.unlink = NULL,
};

void TTYDevice_Register() {
	Mutex_Initialize(&TTYDevice_Mutex);
	struct VFS_Inode *inode = ALLOC_OBJ(struct VFS_Inode);
	if (inode == NULL) {
		KernelLog_ErrorMsg("HAL Console Driver", "Failed to allocate Inode object for terminal inode");
	}
	inode->ctx = NULL;
	inode->ops = &(TTYDevice_InodeOperations);
	if (!DevFS_RegisterInode("halterm", inode)) {
		KernelLog_ErrorMsg("HAL Console Driver",
						   "Failed to register terminal inode in Device Filesystem (path: \"/dev/halterm\")");
	}
}
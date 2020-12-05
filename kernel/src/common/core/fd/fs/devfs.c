#include <common/core/fd/fs/devfs.h>
#include <common/core/proc/mutex.h>
#include <common/lib/dynarray.h>
#include <common/lib/kmsg.h>

struct DevFS_RootDirectoryEntry {
	const char *name;
	size_t hash;
	struct VFS_Inode *inode;
};

static Dynarray(struct DevFS_RootDirectoryEntry) DevFS_RootEntries;
static struct VFS_Inode DevFS_RootInode;
static struct FileOperations DevFS_RootIteratorOperations;
static struct VFS_InodeOperations DevFS_RootInodeOperations;
static struct Mutex DevFS_Mutex;
static struct VFS_Superblock_type DevFS_SuperblockType;

static struct VFS_Superblock *devfs_mount(UNUSED const char *device_path) {
	struct VFS_Superblock *sb = ALLOC_OBJ(struct VFS_Superblock);
	sb->ctx = NULL;
	sb->type = &DevFS_SuperblockType;
	return sb;
}

static bool DevFS_GetInode(UNUSED struct VFS_Superblock *sb, struct VFS_Inode *buf, ino_t id) {
	Mutex_Lock(&DevFS_Mutex);
	if (id == 1) {
		Mutex_Unlock(&DevFS_Mutex);
		memcpy(buf, &DevFS_RootInode, sizeof(*buf));
		return true;
	} else if (id > 1 && id < (ino_t)(2 + DYNARRAY_LENGTH(DevFS_RootEntries))) {
		if (DevFS_RootEntries[id - 2].inode == NULL) {
			Mutex_Unlock(&DevFS_Mutex);
			return false;
		}
		memcpy(buf, DevFS_RootEntries[id - 2].inode, sizeof(*buf));
		Mutex_Unlock(&DevFS_Mutex);
		return true;
	}
	Mutex_Unlock(&DevFS_Mutex);
	return false;
}

bool DevFS_RegisterInode(const char *name, struct VFS_Inode *inode) {
	size_t nameLen = strlen(name);
	if (nameLen > VFS_MAX_NAME_LENGTH) {
		return false;
	}
	char *copy = Heap_AllocateMemory(nameLen + 1);
	if (copy == NULL) {
		return false;
	}
	memcpy(copy, name, nameLen);
	copy[nameLen] = '\0';

	struct DevFS_RootDirectoryEntry entry;
	entry.name = (const char *)copy;
	entry.hash = GetStringHash(entry.name);
	entry.inode = inode;
	Mutex_Lock(&DevFS_Mutex);

	Dynarray(struct DevFS_RootDirectoryEntry) newDynarray = DYNARRAY_PUSH(DevFS_RootEntries, entry);

	if (newDynarray == NULL) {
		Heap_FreeMemory(copy, nameLen + 1);
		Mutex_Unlock(&DevFS_Mutex);
		return false;
	}
	DevFS_RootEntries = newDynarray;
	DevFS_RootInode.stat.stSize = DYNARRAY_LENGTH(DevFS_RootEntries);
	Mutex_Unlock(&DevFS_Mutex);
	return true;
}

static ino_t DevFS_GetRootChild(UNUSED struct VFS_Inode *inode, const char *name) {
	size_t hash = GetStringHash(name);
	Mutex_Lock(&DevFS_Mutex);
	for (size_t i = 0; i < DYNARRAY_LENGTH(DevFS_RootEntries); ++i) {
		if (DevFS_RootEntries[i].hash != hash) {
			continue;
		}
		if (!StringsEqual(DevFS_RootEntries[i].name, name)) {
			continue;
		}
		Mutex_Unlock(&DevFS_Mutex);
		return i + 2;
	}
	Mutex_Unlock(&DevFS_Mutex);
	return 0;
}

static struct File *DeVFS_OpenRoot(UNUSED struct VFS_Inode *inode, UNUSED int perm) {
	struct File *newFile = ALLOC_OBJ(struct File);
	if (newFile == NULL) {
		return NULL;
	}
	newFile->offset = 0;
	newFile->ctx = NULL;
	newFile->ops = &DevFS_RootIteratorOperations;
	return newFile;
}

static int DevFS_ReadRootDirectory(struct File *file, struct DirectoryEntry *buf) {
	Mutex_Lock(&DevFS_Mutex);
	off_t index = file->offset;
	if (index >= DYNARRAY_LENGTH(DevFS_RootEntries)) {
		Mutex_Unlock(&DevFS_Mutex);
		return 0;
	};
	buf->dtIno = index + 2;
	memcpy(buf->dtName, DevFS_RootEntries[index].name, strlen(DevFS_RootEntries[index].name));
	file->offset++;
	Mutex_Unlock(&DevFS_Mutex);
	return 1;
}

static void DevFS_CloseRootDirectoryIterator(struct File *file) {
	VFS_FinalizeFile(file);
}

void DevFS_Initialize() {
	DevFS_RootEntries = DYNARRAY_NEW(struct DevFS_RootDirectoryEntry);
	if (DevFS_RootEntries == NULL) {
		KernelLog_ErrorMsg("Device Filesystem (devfs)", "Failed to allocate dynarrays for devfs usage");
	}
	Mutex_Initialize(&DevFS_Mutex);

	memcpy(&(DevFS_SuperblockType.fsName), "devfs", 6);
	DevFS_SuperblockType.fsNameHash = GetStringHash("devfs");
	DevFS_SuperblockType.getInode = DevFS_GetInode;
	DevFS_SuperblockType.dropInode = NULL;
	DevFS_SuperblockType.sync = NULL;
	DevFS_SuperblockType.mount = devfs_mount;
	DevFS_SuperblockType.umount = NULL;
	DevFS_SuperblockType.getRootInode = NULL;

	DevFS_RootInode.ctx = NULL;
	DevFS_RootInode.stat.stSize = 0;
	DevFS_RootInode.stat.stType = VFS_DT_DIR;
	DevFS_RootInode.stat.stBlksize = 0;
	DevFS_RootInode.stat.stBlkcnt = 0;
	DevFS_RootInode.ops = &DevFS_RootInodeOperations;

	DevFS_RootInodeOperations.getChild = DevFS_GetRootChild;
	DevFS_RootInodeOperations.open = DeVFS_OpenRoot;
	DevFS_RootInodeOperations.mkdir = NULL;
	DevFS_RootInodeOperations.link = NULL;
	DevFS_RootInodeOperations.unlink = NULL;

	DevFS_RootIteratorOperations.read = NULL;
	DevFS_RootIteratorOperations.write = NULL;
	DevFS_RootIteratorOperations.lseek = NULL;
	DevFS_RootIteratorOperations.flush = NULL;
	DevFS_RootIteratorOperations.close = DevFS_CloseRootDirectoryIterator;
	DevFS_RootIteratorOperations.readdir = DevFS_ReadRootDirectory;

	VFS_RegisterFilesystem(&DevFS_SuperblockType);
}
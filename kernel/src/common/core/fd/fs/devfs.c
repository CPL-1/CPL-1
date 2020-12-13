#include <common/core/fd/fs/devfs.h>
#include <common/core/proc/mutex.h>
#include <common/lib/dynarray.h>
#include <common/lib/kmsg.h>

struct DevFS_RootDirectoryEntry {
	const char *name;
	size_t hash;
	struct VFS_Inode *inode;
};

static Dynarray(struct DevFS_RootDirectoryEntry) m_rootEntries;
static struct VFS_Inode m_rootInode;
static struct FileOperations m_rootIteratorOperations;
static struct VFS_InodeOperations m_rootInodeOperations;
static struct Mutex m_mutex;
static struct VFS_Superblock_type m_superblockType;

static struct VFS_Superblock *devfs_mount(MAYBE_UNUSED const char *device_path) {
	struct VFS_Superblock *sb = ALLOC_OBJ(struct VFS_Superblock);
	sb->ctx = NULL;
	sb->type = &m_superblockType;
	return sb;
}

static bool DevFS_GetInode(MAYBE_UNUSED struct VFS_Superblock *sb, struct VFS_Inode *buf, ino_t id) {
	Mutex_Lock(&m_mutex);
	if (id == 1) {
		Mutex_Unlock(&m_mutex);
		memcpy(buf, &m_rootInode, sizeof(*buf));
		return true;
	} else if (id > 1 && id < (ino_t)(2 + DYNARRAY_LENGTH(m_rootEntries))) {
		if (m_rootEntries[id - 2].inode == NULL) {
			Mutex_Unlock(&m_mutex);
			return false;
		}
		memcpy(buf, m_rootEntries[id - 2].inode, sizeof(*buf));
		Mutex_Unlock(&m_mutex);
		return true;
	}
	Mutex_Unlock(&m_mutex);
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
	Mutex_Lock(&m_mutex);

	Dynarray(struct DevFS_RootDirectoryEntry) newDynarray = DYNARRAY_PUSH(m_rootEntries, entry);

	if (newDynarray == NULL) {
		Heap_FreeMemory(copy, nameLen + 1);
		Mutex_Unlock(&m_mutex);
		return false;
	}
	m_rootEntries = newDynarray;
	m_rootInode.stat.stSize = DYNARRAY_LENGTH(m_rootEntries);
	Mutex_Unlock(&m_mutex);
	return true;
}

static ino_t DevFS_GetRootChild(MAYBE_UNUSED struct VFS_Inode *inode, const char *name) {
	size_t hash = GetStringHash(name);
	Mutex_Lock(&m_mutex);
	for (size_t i = 0; i < DYNARRAY_LENGTH(m_rootEntries); ++i) {
		if (m_rootEntries[i].hash != hash) {
			continue;
		}
		if (!StringsEqual(m_rootEntries[i].name, name)) {
			continue;
		}
		Mutex_Unlock(&m_mutex);
		return i + 2;
	}
	Mutex_Unlock(&m_mutex);
	return 0;
}

static struct File *DeVFS_OpenRoot(MAYBE_UNUSED struct VFS_Inode *inode, MAYBE_UNUSED int perm) {
	struct File *newFile = ALLOC_OBJ(struct File);
	if (newFile == NULL) {
		return NULL;
	}
	newFile->offset = 0;
	newFile->ctx = NULL;
	newFile->ops = &m_rootIteratorOperations;
	newFile->isATTY = false;
	return newFile;
}

static int DevFS_ReadRootDirectory(struct File *file, struct DirectoryEntry *buf) {
	Mutex_Lock(&m_mutex);
	off_t index = file->offset;
	if (index >= DYNARRAY_LENGTH(m_rootEntries)) {
		Mutex_Unlock(&m_mutex);
		return 0;
	};
	buf->dtIno = index + 2;
	memcpy(buf->dtName, m_rootEntries[index].name, strlen(m_rootEntries[index].name));
	file->offset++;
	Mutex_Unlock(&m_mutex);
	return 1;
}

static void DevFS_CloseRootDirectoryIterator(struct File *file) {
	VFS_FinalizeFile(file);
}

void DevFS_Initialize() {
	m_rootEntries = DYNARRAY_NEW(struct DevFS_RootDirectoryEntry);
	if (m_rootEntries == NULL) {
		KernelLog_ErrorMsg("Device Filesystem (devfs)", "Failed to allocate dynarrays for devfs usage");
	}
	Mutex_Initialize(&m_mutex);

	memcpy(&(m_superblockType.fsName), "devfs", 6);
	m_superblockType.fsNameHash = GetStringHash("devfs");
	m_superblockType.getInode = DevFS_GetInode;
	m_superblockType.dropInode = NULL;
	m_superblockType.sync = NULL;
	m_superblockType.mount = devfs_mount;
	m_superblockType.umount = NULL;
	m_superblockType.getRootInode = NULL;

	m_rootInode.ctx = NULL;
	m_rootInode.stat.stSize = 0;
	m_rootInode.stat.stType = VFS_DT_DIR;
	m_rootInode.stat.stBlksize = 0;
	m_rootInode.stat.stBlkcnt = 0;
	m_rootInode.ops = &m_rootInodeOperations;

	m_rootInodeOperations.getChild = DevFS_GetRootChild;
	m_rootInodeOperations.open = DeVFS_OpenRoot;
	m_rootInodeOperations.mkdir = NULL;
	m_rootInodeOperations.link = NULL;
	m_rootInodeOperations.unlink = NULL;

	m_rootIteratorOperations.read = NULL;
	m_rootIteratorOperations.write = NULL;
	m_rootIteratorOperations.lseek = NULL;
	m_rootIteratorOperations.flush = NULL;
	m_rootIteratorOperations.close = DevFS_CloseRootDirectoryIterator;
	m_rootIteratorOperations.readdir = DevFS_ReadRootDirectory;

	VFS_RegisterFilesystem(&m_superblockType);
}
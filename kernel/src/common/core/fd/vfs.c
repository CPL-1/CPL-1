#include <common/core/fd/vfs.h>
#include <common/core/memory/heap.h>
#include <common/lib/kmsg.h>
#include <common/lib/pathsplit.h>

static struct Mutex m_mutex;
static struct VFS_Superblock *m_root;
static struct VFS_Superblock *m_superblockList;
static struct VFS_Superblock_type *m_superblockTypes;

struct VFS_Inode *VFS_GetInode(struct VFS_Superblock *sb, ino_t id) {
	if (sb->type->getInode == NULL) {
		return NULL;
	}
	Mutex_Lock(&(sb->mutex));
	ino_t modulo = id % VFS_ICACHE_MODULO;
	struct VFS_Inode *current = sb->inodeLists[modulo];
	while (current != NULL) {
		if (current->id == id) {
			current->refCount++;
			Mutex_Unlock(&(sb->mutex));
			return current;
		}
		current = current->nextInCache;
	}
	struct VFS_Inode *node = ALLOC_OBJ(struct VFS_Inode);
	if (node == NULL) {
		Mutex_Unlock(&(sb->mutex));
		return NULL;
	}
	if (!sb->type->getInode(sb, node, id)) {
		FREE_OBJ(node);
		Mutex_Unlock(&(sb->mutex));
		return NULL;
	}
	node->nextInCache = sb->inodeLists[modulo];
	if (sb->inodeLists[modulo] != NULL) {
		sb->inodeLists[modulo]->prevInCache = node;
	}
	node->prevInCache = NULL;
	sb->inodeLists[modulo] = node;
	node->id = id;
	node->refCount = 1;
	node->dirty = false;
	node->sb = sb;
	node->mount = NULL;
	Mutex_Initialize(&(node->mutex));
	Mutex_Unlock(&(sb->mutex));
	return node;
}

void VFS_DropInode(struct VFS_Superblock *sb, struct VFS_Inode *inode) {
	Mutex_Lock(&(sb->mutex));
	ino_t modulo = inode->id % VFS_ICACHE_MODULO;
	if (inode->refCount == 0) {
		KernelLog_ErrorMsg("Virtual File System", "How the hell we are dropping inode more times than we opened it?");
	}
	inode->refCount--;
	if (inode->refCount == 0) {
		if (inode->prevInCache == NULL) {
			sb->inodeLists[modulo] = inode->nextInCache;
		} else {
			inode->prevInCache->nextInCache = inode->nextInCache;
		}
		if (inode->nextInCache != NULL) {
			inode->nextInCache->prevInCache = NULL;
		}
		if (sb->type->dropInode != NULL) {
			sb->type->dropInode(sb, inode, inode->id);
		}
		FREE_OBJ(inode);
	}
	Mutex_Unlock(&(sb->mutex));
}

struct VFS_Inode *VFS_SuperblockGetNextInode(struct VFS_Superblock *sb, struct VFS_Inode *inode) {
	if (inode == NULL) {
		for (ino_t i = 0; i < VFS_ICACHE_MODULO; ++i) {
			if (sb->inodeLists[i] != NULL) {
				return sb->inodeLists[i];
			}
		}
	} else {
		if (inode->nextInCache != NULL) {
			return inode->nextInCache;
		}
		ino_t modulo = inode->id % VFS_ICACHE_MODULO;
		for (ino_t i = modulo; i < VFS_ICACHE_MODULO; ++i) {
			return sb->inodeLists[i];
		}
	}
	return NULL;
}

bool VFS_IsInodeLoaded(struct VFS_Superblock *sb, ino_t id) {
	ino_t modulo = id % VFS_ICACHE_MODULO;
	struct VFS_Inode *current = sb->inodeLists[modulo];
	while (current != NULL) {
		if (current->id == id) {
			return true;
		}
		current = current->nextInCache;
	}
	return false;
}

bool VFS_DropMount(struct VFS_Dentry *dentry) {
	struct VFS_Superblock *sb = dentry->inode->sb;
	struct VFS_Dentry *mountLocation = sb->mountLocation;
	size_t a = 0;
	if ((a = ATOMIC_DECREMENT(&(dentry->refCount))) > 1) {
		Mutex_Unlock(&(dentry->mutex));
		return false;
	}
	mountLocation->inode->mount = NULL;
	sb->mountLocation = NULL;
	Mutex_Unlock(&(dentry->mutex));
	if (sb->type->umount != NULL) {
		sb->type->umount(sb->ctx);
	}
	Mutex_Lock(&m_mutex);
	if (sb->nextMounted != NULL) {
		sb->nextMounted->prevMounted = sb->prevMounted;
	}
	if (sb->prevMounted != NULL) {
		sb->prevMounted->nextMounted = sb->nextMounted;
	} else {
		m_superblockList = sb->nextMounted;
	}
	Mutex_Unlock(&m_mutex);
	FREE_OBJ(sb);
	return true;
}

struct VFS_Dentry *VFS_BacktraceMounts(struct VFS_Dentry *dentry) {
	while (dentry->parent == NULL) {
		struct VFS_Superblock *sb = dentry->inode->sb;
		struct VFS_Dentry *mountLocation = sb->mountLocation;
		if (sb->mountLocation == NULL) {
			return dentry;
		}
		ATOMIC_INCREMENT(&(mountLocation->refCount));
		Mutex_Lock(&(mountLocation->mutex));
		VFS_DropMount(dentry);
		dentry = mountLocation;
	}
	return dentry;
}

void VFS_CutDentryFromChildList(struct VFS_Dentry *dentry) {
	struct VFS_Dentry *prev = dentry->prev;
	struct VFS_Dentry *next = dentry->next;
	struct VFS_Dentry *par = dentry->parent;
	if (prev == NULL) {
		par->head = next;
	} else {
		prev->next = next;
	}
	if (next != NULL) {
		next->prev = prev;
	}
}

bool VFS_DropDentry(struct VFS_Dentry *dentry) {
	if (ATOMIC_DECREMENT(&(dentry->refCount)) > 1) {
		Mutex_Unlock(&(dentry->mutex));
		return false;
	}
	VFS_CutDentryFromChildList(dentry);
	VFS_DropInode(dentry->inode->sb, dentry->inode);
	Mutex_Unlock(&(dentry->mutex));
	FREE_OBJ(dentry);
	return true;
}

struct VFS_Dentry *VFS_WalkToDentryParent(struct VFS_Dentry *dentry) {
	struct VFS_Dentry *backtraced = VFS_BacktraceMounts(dentry);
	if (backtraced->parent == NULL) {
		return dentry;
	}
	struct VFS_Dentry *parent = backtraced->parent;
	ATOMIC_INCREMENT(&(parent->refCount));
	Mutex_Lock(&(parent->mutex));
	VFS_DropDentry(backtraced);
	return parent;
}

struct VFS_Dentry *VFS_GetLoadedDentryChild(struct VFS_Dentry *dentry, const char *name) {
	size_t hash = GetStringHash(name);
	struct VFS_Dentry *current = dentry->head;
	while (current != NULL) {
		if (current->hash != hash) {
			current = current->next;
			continue;
		}
		if (StringsEqual(current->name, name)) {
			return current;
		}
		current = current->next;
	}
	return NULL;
}

struct VFS_Dentry *VFS_DentryLoadChild(struct VFS_Dentry *dentry, const char *name) {
	size_t nameLength = strlen(name);
	if (nameLength > 255) {
		return NULL;
	}
	struct VFS_Superblock *sb = dentry->inode->sb;
	if (dentry->inode->ops->getChild == NULL) {
		return NULL;
	}
	ino_t id = dentry->inode->ops->getChild(dentry->inode, name);
	if (id == 0) {
		return NULL;
	}
	struct VFS_Inode *newInode = VFS_GetInode(sb, id);
	if (newInode == NULL) {
		return NULL;
	}
	struct VFS_Dentry *newDentry = ALLOC_OBJ(struct VFS_Dentry);
	if (dentry == NULL) {
		return NULL;
	}
	newDentry->inode = newInode;
	newDentry->hash = GetStringHash(name);
	memset(newDentry->name, 0, 255);
	memcpy(newDentry->name, name, nameLength);
	newDentry->head = NULL;
	newDentry->refCount = 0;
	ATOMIC_INCREMENT(&(dentry->refCount));
	newDentry->prev = NULL;
	newDentry->next = dentry->head;
	newDentry->parent = dentry;
	newDentry->next = dentry->head;
	if (dentry->head != NULL) {
		dentry->head->prev = newDentry;
	}
	dentry->head = newDentry;
	Mutex_Initialize(&(newDentry->mutex));
	return newDentry;
}

struct VFS_Dentry *VFS_TraceMounts(struct VFS_Dentry *dentry) {
	while (true) {
		struct VFS_Superblock *sb = dentry->inode->mount;
		if (sb == NULL) {
			break;
		}
		struct VFS_Dentry *next = sb->root;
		ATOMIC_INCREMENT(&(next->refCount));
		Mutex_Unlock(&(dentry->mutex));
		Mutex_Lock(&(next->mutex));
		ATOMIC_DECREMENT(&(dentry->refCount));
		dentry = next;
	}
	return dentry;
}

struct VFS_Dentry *VFS_Dentry_GetDentryDropParent(struct VFS_Dentry *dentry) {
	if (dentry->parent != NULL) {
		return dentry->parent;
	} else {
		return dentry->inode->sb->mountLocation;
	}
}

void VFS_Dentry_DropRecursively(struct VFS_Dentry *dentry) {
	while (dentry != NULL) {
		struct VFS_Dentry *parent = VFS_Dentry_GetDentryDropParent(dentry);
		if (parent == NULL) {
			ATOMIC_DECREMENT(&(dentry->refCount));
			Mutex_Unlock(&(dentry->mutex));
			return;
		}
		Mutex_Lock(&(parent->mutex));
		if (!((dentry->parent == NULL) ? VFS_DropMount : VFS_DropDentry)(dentry)) {
			Mutex_Unlock(&(dentry->mutex));
			Mutex_Unlock(&(parent->mutex));
			return;
		}
		dentry = parent;
	}
}

struct VFS_Dentry *VFS_Dentry_WalkToChild(struct VFS_Dentry *dentry, const char *name) {
	if (strlen(name) > VFS_MAX_NAME_LENGTH) {
		VFS_Dentry_DropRecursively(dentry);
		return NULL;
	}
	struct VFS_Dentry *result = VFS_GetLoadedDentryChild(dentry, name);
	if (result == NULL) {
		result = VFS_DentryLoadChild(dentry, name);
		if (result == NULL) {
			VFS_Dentry_DropRecursively(dentry);
			return NULL;
		}
	}
	ATOMIC_INCREMENT(&(result->refCount));
	Mutex_Unlock(&(dentry->mutex));
	Mutex_Lock(&(result->mutex));
	ATOMIC_DECREMENT(&(dentry->refCount));
	return VFS_TraceMounts(result);
}

struct VFS_Dentry *VFS_Dentry_Walk(struct VFS_Dentry *dentry, struct PathSplitter *splitter) {
	const char *name = PathSplitter_Get(splitter);
	struct VFS_Dentry *current = dentry;
	while (name != NULL) {
		if (*name == '\0' || StringsEqual(name, ".")) {
			goto next;
		} else if (StringsEqual(name, "..")) {
			current = VFS_WalkToDentryParent(current);
		} else {
			current = VFS_Dentry_WalkToChild(current, name);
		}
		if (current == NULL) {
			return NULL;
		}
	next:
		name = PathSplitter_Advance(splitter);
	}
	return current;
}

struct VFS_Dentry *VFS_Dentry_WalkFromRoot(const char *path) {
	struct PathSplitter splitter;
	if (!PathSplitter_Init(path, &splitter)) {
		return NULL;
	}
	struct VFS_Dentry *fsRoot = m_root->root;
	if (fsRoot == NULL) {
		KernelLog_ErrorMsg("Virtual File System", "Filesystem root should never be NULL");
	}
	Mutex_Lock(&(fsRoot->mutex));
	ATOMIC_INCREMENT(&(fsRoot->refCount));
	fsRoot = VFS_TraceMounts(fsRoot);
	if (fsRoot == NULL) {
		KernelLog_ErrorMsg("Virtual File System", "VFS_TraceMounts should never return NULL");
	}
	struct VFS_Dentry *result = VFS_Dentry_Walk(fsRoot, &splitter);
	PathSplitter_Dispose(&splitter);
	return result;
}

bool VFS_Dentry_MountInitializedFS(const char *path, struct VFS_Superblock *sb) {
	struct VFS_Dentry *dir = VFS_Dentry_WalkFromRoot(path);
	if (dir == NULL) {
		VFS_Dentry_DropRecursively(dir);
		return false;
	}
	if (dir->inode->stat.stType != VFS_DT_DIR) {
		VFS_Dentry_DropRecursively(dir);
		return false;
	}
	if (dir->inode->mount != NULL) {
		KernelLog_ErrorMsg("Virtual File System", "VFS_TraceMounts is not doing its job");
	}
	dir->inode->mount = sb;
	sb->mountLocation = dir;
	ino_t root_id = 1;
	if (sb->type->getRootInode != NULL) {
		root_id = sb->type->getRootInode(sb->ctx);
	}
	if (root_id == 0) {
		VFS_Dentry_DropRecursively(dir);
		return false;
	}
	for (size_t i = 0; i < VFS_ICACHE_MODULO; ++i) {
		sb->inodeLists[i] = NULL;
	}
	Mutex_Initialize(&(sb->mutex));
	struct VFS_Inode *inode = VFS_GetInode(sb, root_id);
	if (inode == NULL) {
		VFS_Dentry_DropRecursively(dir);
		return false;
	}
	struct VFS_Dentry *dentry = ALLOC_OBJ(struct VFS_Dentry);
	dentry->hash = 0;
	if (dentry == NULL) {
		VFS_Dentry_DropRecursively(dir);
		VFS_DropInode(inode->sb, inode);
		return false;
	}
	dentry->parent = dentry->head = dentry->next = dentry->prev = NULL;
	dentry->refCount = 1;
	dentry->inode = inode;
	Mutex_Initialize(&(dentry->mutex));
	sb->root = dentry;
	Mutex_Lock(&m_mutex);
	sb->nextMounted = m_superblockList;
	if (m_superblockList != NULL) {
		m_superblockList->prevMounted = sb;
	}
	sb->prevMounted = NULL;
	m_superblockList = sb;
	Mutex_Unlock(&m_mutex);
	Mutex_Unlock(&(dir->mutex));
	return true;
}

struct VFS_Superblock_type *VFS_Dentry_GetFSTypeDescriptor(const char *fsType) {
	Mutex_Lock(&m_mutex);
	struct VFS_Superblock_type *current = m_superblockTypes;
	while (current != NULL) {
		if (StringsEqual(current->fsName, fsType)) {
			Mutex_Unlock(&m_mutex);
			if (current->mount == NULL) {
				return NULL;
			}
			return current;
		}
		current = current->next;
	}
	Mutex_Unlock(&m_mutex);
	return NULL;
}

bool VFS_UserMount(const char *path, const char *devpath, const char *fsTypeName) {
	struct VFS_Superblock_type *fsType = VFS_Dentry_GetFSTypeDescriptor(fsTypeName);
	if (fsType == NULL) {
		return false;
	}
	if (fsType->mount == NULL) {
		return false;
	}
	struct VFS_Superblock *sb = fsType->mount(devpath);
	if (sb == NULL) {
		return false;
	}
	if (!VFS_Dentry_MountInitializedFS(path, sb)) {
		if (!(fsType->umount != NULL)) {
			fsType->umount(sb);
		}
		FREE_OBJ(sb);
		return false;
	}
	return true;
}

bool VFS_UserUnmount(const char *path) {
	struct VFS_Dentry *fsRoot = VFS_Dentry_WalkFromRoot(path);
	if (fsRoot == NULL) {
		return false;
	}
	struct VFS_Dentry *location = fsRoot->inode->sb->mountLocation;
	Mutex_Lock(&(location->mutex));
	struct VFS_Superblock *mounted_sb = location->inode->mount;
	if (mounted_sb == NULL) {
		Mutex_Unlock(&(location->mutex));
		VFS_Dentry_DropRecursively(fsRoot);
		return false;
	}
	location->inode->mount = NULL;
	ATOMIC_DECREMENT(&(fsRoot->refCount));
	Mutex_Unlock(&(location->mutex));
	VFS_Dentry_DropRecursively(fsRoot);
	return true;
}

void VFS_Initialize(struct VFS_Superblock *sb) {
	for (size_t i = 0; i < VFS_ICACHE_MODULO; ++i) {
		sb->inodeLists[i] = NULL;
	}
	ino_t rootInode = 1;
	Mutex_Initialize(&m_mutex);
	if (sb->type->getRootInode != NULL) {
		rootInode = sb->type->getRootInode(sb->ctx);
	}
	if (rootInode == 0) {
		KernelLog_ErrorMsg("Virtual File System", "rootfs root inode number is zero");
	}
	struct VFS_Inode *inode = VFS_GetInode(sb, rootInode);
	if (inode == NULL) {
		KernelLog_ErrorMsg("Virtual File System", "Failed to load rootfs root inode");
	}
	struct VFS_Dentry *dentry = ALLOC_OBJ(struct VFS_Dentry);
	if (dentry == NULL) {
		KernelLog_ErrorMsg("Virtual File System", "Failed to allocate root dirent");
	}
	dentry->parent = dentry->head = dentry->prev = dentry->next = NULL;
	dentry->hash = 0;
	dentry->inode = inode;
	dentry->refCount = 1;
	sb->root = dentry;
	sb->mountLocation = NULL;
	sb->nextMounted = sb->prevMounted;
	m_superblockList = sb;
	m_root = sb;
	Mutex_Initialize(&(dentry->mutex));
}

void VFS_RegisterFilesystem(struct VFS_Superblock_type *type) {
	Mutex_Lock(&m_mutex);
	struct VFS_Superblock_type *current = m_superblockTypes;
	type->next = current;
	if (current != NULL) {
		current->prev = type;
	}
	type->prev = NULL;
	m_superblockTypes = type;
	Mutex_Unlock(&m_mutex);
}

struct File *VFS_Open(const char *path, int perm) {
	struct VFS_Dentry *file = VFS_Dentry_WalkFromRoot(path);
	if (file == NULL) {
		return false;
	}
	if (file->inode->ops->open == NULL) {
		VFS_Dentry_DropRecursively(file);
	}
	Mutex_Unlock(&(file->mutex));
	struct File *fd = file->inode->ops->open(file->inode, perm);
	if (fd == NULL) {
		Mutex_Lock(&(file->mutex));
		VFS_Dentry_DropRecursively(file);
	}
	fd->dentry = file;
	Mutex_Initialize(&(fd->mutex));
	fd->refCount = 1;
	return fd;
}

void VFS_FinalizeFile(struct File *fd) {
	Mutex_Lock(&(fd->dentry->mutex));
	VFS_Dentry_DropRecursively(fd->dentry);
}
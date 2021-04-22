#include <common/core/fd/fs/rootfs.h>
#include <common/core/memory/heap.h>
#include <common/lib/kmsg.h>

static struct VFS_InodeOperations m_inodeOperations;
static struct VFS_Superblock_type m_superblockType;

#define ROOTFS_ROOT_INODE_CTX ((void *)1)
#define ROOTFS_DEV_INODE_CTX ((void *)2)

static ino_t RootFS_GetChild(struct VFS_Inode *inode, const char *name) {
	if (inode->ctx == ROOTFS_ROOT_INODE_CTX) {
		if (StringsEqual(name, "dev")) {
			return 2;
		}
	}
	return 0;
}

static bool RootFS_GetInode(MAYBE_UNUSED struct VFS_Superblock *sb, struct VFS_Inode *inode, ino_t id) {
	inode->stat.st_type = VFS_DT_DIR;
	inode->stat.st_size = 0;
	inode->stat.st_blksize = 0;
	inode->stat.st_blkcnt = 0;
	inode->ops = &m_inodeOperations;
	if (id == 1) {
		inode->ctx = ROOTFS_ROOT_INODE_CTX;
		return true;
	} else if (id == 2) {
		inode->ctx = ROOTFS_DEV_INODE_CTX;
		return true;
	}
	return false;
}

struct VFS_Superblock *RootFS_MakeSuperblock() {
	memcpy(&(m_superblockType.fsName), "rootfs", 7);
	m_superblockType.fsNameHash = GetStringHash("rootfs");
	m_superblockType.mount = NULL;
	m_superblockType.umount = NULL;
	m_superblockType.getInode = RootFS_GetInode;
	m_superblockType.getRootInode = NULL;
	m_superblockType.dropInode = NULL;
	m_superblockType.sync = NULL;

	m_inodeOperations.getChild = RootFS_GetChild;
	m_inodeOperations.open = NULL;
	m_inodeOperations.mkdir = NULL;
	m_inodeOperations.link = NULL;
	m_inodeOperations.unlink = NULL;

	struct VFS_Superblock *sb = ALLOC_OBJ(struct VFS_Superblock);
	if (sb == NULL) {
		KernelLog_ErrorMsg("Root Filesystem (rootfs)", "Failed to allocate space for rootfs superblock");
	}
	sb->type = &m_superblockType;
	sb->ctx = NULL;
	Mutex_Initialize(&(sb->mutex));
	return sb;
}

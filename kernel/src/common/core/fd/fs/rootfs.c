#include <common/core/fd/fs/rootfs.h>
#include <common/core/memory/heap.h>
#include <common/lib/kmsg.h>

static struct VFS_InodeOperations RootFS_InodeOperations;
static struct VFS_Superblock_type RootFS_SuperblockType;

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

static bool RootFS_GetInode(UNUSED struct VFS_Superblock *sb, struct VFS_Inode *inode, ino_t id) {
	inode->stat.stType = VFS_DT_DIR;
	inode->stat.stSize = 0;
	inode->stat.stBlksize = 0;
	inode->stat.stBlkcnt = 0;
	inode->ops = &RootFS_InodeOperations;
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
	memcpy(&(RootFS_SuperblockType.fsName), "rootfs", 7);
	RootFS_SuperblockType.fsNameHash = GetStringHash("rootfs");
	RootFS_SuperblockType.mount = NULL;
	RootFS_SuperblockType.umount = NULL;
	RootFS_SuperblockType.getInode = RootFS_GetInode;
	RootFS_SuperblockType.getRootInode = NULL;
	RootFS_SuperblockType.dropInode = NULL;
	RootFS_SuperblockType.sync = NULL;

	RootFS_InodeOperations.getChild = RootFS_GetChild;
	RootFS_InodeOperations.open = NULL;
	RootFS_InodeOperations.mkdir = NULL;
	RootFS_InodeOperations.link = NULL;
	RootFS_InodeOperations.unlink = NULL;

	struct VFS_Superblock *sb = ALLOC_OBJ(struct VFS_Superblock);
	if (sb == NULL) {
		KernelLog_ErrorMsg("Root Filesystem (rootfs)", "Failed to allocate space for rootfs superblock");
	}
	sb->type = &RootFS_SuperblockType;
	sb->ctx = NULL;
	return sb;
}
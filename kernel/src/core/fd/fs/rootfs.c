#include <core/fd/fs/rootfs.h>
#include <core/memory/heap.h>
#include <lib/kmsg.h>

static struct vfs_inode_ops rootfs_inode_ops;
static struct vfs_superblock_type rootfs_sb_type;

#define ROOTFS_ROOT_INODE_CTX ((void *)1)
#define ROOTFS_DEV_INODE_CTX ((void *)2)

static int rootfs_get_child(struct vfs_inode *inode, const char *name) {
	if (inode->ctx == ROOTFS_ROOT_INODE_CTX) {
		if (streq(name, "dev")) {
			return 2;
		}
	}
	return 0;
}

static bool rootfs_get_inode(unused struct vfs_superblock *sb,
							 struct vfs_inode *inode, int id) {
	inode->stat.st_type = VFS_DT_DIR;
	inode->stat.st_size = 0;
	inode->stat.st_blksize = 0;
	inode->stat.st_blkcnt = 0;
	inode->ops = &rootfs_inode_ops;
	if (id == 1) {
		inode->ctx = ROOTFS_ROOT_INODE_CTX;
		return true;
	} else if (id == 2) {
		inode->ctx = ROOTFS_DEV_INODE_CTX;
		return true;
	}
	return false;
}

struct vfs_superblock *rootfs_make_superblock() {
	memcpy(&(rootfs_sb_type.fs_name), "rootfs", 7);
	rootfs_sb_type.fs_name_hash = strhash("rootfs");
	rootfs_sb_type.mount = NULL;
	rootfs_sb_type.umount = NULL;
	rootfs_sb_type.get_inode = rootfs_get_inode;
	rootfs_sb_type.get_root_inode = NULL;
	rootfs_sb_type.drop_inode = NULL;
	rootfs_sb_type.sync = NULL;

	rootfs_inode_ops.get_child = rootfs_get_child;
	rootfs_inode_ops.open = NULL;
	rootfs_inode_ops.mkdir = NULL;
	rootfs_inode_ops.link = NULL;
	rootfs_inode_ops.unlink = NULL;

	struct vfs_superblock *sb = ALLOC_OBJ(struct vfs_superblock);
	if (sb == NULL) {
		kmsg_err("Root Filesystem (rootfs)",
				 "Failed to allocate space for rootfs superblock");
	}
	sb->type = &rootfs_sb_type;
	sb->ctx = NULL;
	return sb;
}
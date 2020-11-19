#ifndef __VFS_H_INCLUDED__
#define __VFS_H_INCLUDED__

#include <core/fd/fd.h>
#include <core/proc/mutex.h>

enum {
	VFS_DT_UNKNOWN = 0,
	VFS_DT_FIFO = 1,
	VFS_DT_CHR = 2,
	VFS_DT_DIR = 4,
	VFS_DT_BLK = 6,
	VFS_DT_REG = 8,
	VFS_DT_LNK = 9,
	VFS_DT_SOCK = 12,
	VFS_DT_WHT = 14,
};

struct vfs_dentry {
	char name[VFS_MAX_NAME_LENGTH + 1];
	size_t hash;
	size_t ref_count;
	struct vfs_inode *inode;
	struct mutex mutex;
	struct vfs_dentry *parent, *head, *prev, *next;
};

struct vfs_inode_ops {
	int (*get_child)(struct vfs_inode *inode, const char *name);
	struct fd *(*open)(struct vfs_inode *inode, int perm);
	int (*mkdir)(struct vfs_inode *inode, const char *name);
	int (*link)(struct vfs_inode *inode, int id);
	int (*unlink)(struct vfs_inode *inode, const char *name);
};

struct vfs_stat {
	mode_t st_type;
	off_t st_size;
	blksize_t st_blksize;
	blkcnt_t st_blkcnt;
};

struct vfs_inode {
	bool dirty;
	int id;
	void *ctx;
	size_t ref_count;
	struct vfs_inode *prev_in_cache, *next_in_cache;
	struct vfs_stat stat;
	struct vfs_superblock *mount;
	struct vfs_superblock *sb;
	struct vfs_inode_ops *ops;
	struct mutex mutex;
};

struct vfs_superblock_type {
	char fs_name[255];
	size_t fs_name_hash;
	struct vfs_superblock *(*mount)(const char *device_path);
	bool (*get_inode)(struct vfs_superblock *sb, struct vfs_inode *buf, int id);
	void (*drop_inode)(struct vfs_superblock *sb, int id);
	void (*sync)(struct vfs_superblock *sb);
	void (*umount)(struct vfs_superblock *sb);
	int (*get_root_inode)(struct vfs_superblock *sb);
	struct vfs_superblock_type *prev, *next;
};

#define VFS_ICACHE_MODULO 25
struct vfs_superblock {
	struct vfs_dentry *root;
	void *ctx;
	struct vfs_superblock_type *type;
	struct vfs_superblock *next;
	struct vfs_inode *inode_lists[VFS_ICACHE_MODULO];
	struct vfs_superblock *prev_mounted, *next_mounted;
	struct vfs_dentry *mount_location;
	struct mutex mutex;
};

void vfs_init(struct vfs_superblock *sb);

bool vfs_mount_initialized(const char *path, struct vfs_superblock *sb);
bool vfs_mount_user(const char *path, const char *devpath,
					const char *fs_type_name);
bool vfs_unmount(const char *path);
void vfs_register_filesystem(struct vfs_superblock_type *type);

struct fd *vfs_open(const char *path, int perm);
void vfs_finalize(struct fd *fd);

#endif
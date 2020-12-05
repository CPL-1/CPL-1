#ifndef __VFS_H_INCLUDED__
#define __VFS_H_INCLUDED__

#include <common/core/fd/fd.h>
#include <common/core/proc/mutex.h>

enum
{
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

enum
{
	VFS_O_RDONLY = 0,
	VFS_O_WRONLY = 1,
	VFS_O_RDWR = 2,
	VFS_O_ACCMODE = 3
};

struct VFS_Dentry {
	char name[VFS_MAX_NAME_LENGTH + 1];
	size_t hash;
	size_t refCount;
	struct VFS_Inode *inode;
	struct Mutex mutex;
	struct VFS_Dentry *parent, *head, *prev, *next;
};

struct VFS_InodeOperations {
	ino_t (*getChild)(struct VFS_Inode *inode, const char *name);
	struct File *(*open)(struct VFS_Inode *inode, int perm);
	int (*mkdir)(struct VFS_Inode *inode, const char *name);
	int (*link)(struct VFS_Inode *inode, ino_t id);
	int (*unlink)(struct VFS_Inode *inode, const char *name);
};

struct VFS_Stat {
	mode_t stType;
	off_t stSize;
	blksize_t stBlksize;
	blkcnt_t stBlkcnt;
};

struct VFS_Inode {
	bool dirty;
	ino_t id;
	void *ctx;
	size_t refCount;
	struct VFS_Inode *prevInCache, *nextInCache;
	struct VFS_Stat stat;
	struct VFS_Superblock *mount;
	struct VFS_Superblock *sb;
	struct VFS_InodeOperations *ops;
	struct Mutex mutex;
};

struct VFS_Superblock_type {
	char fsName[255];
	size_t fsNameHash;
	struct VFS_Superblock *(*mount)(const char *device_path);
	bool (*getInode)(struct VFS_Superblock *sb, struct VFS_Inode *buf, ino_t id);
	void (*dropInode)(struct VFS_Superblock *sb, struct VFS_Inode *buf, ino_t id);
	void (*sync)(struct VFS_Superblock *sb);
	void (*umount)(struct VFS_Superblock *sb);
	ino_t (*getRootInode)(struct VFS_Superblock *sb);
	struct VFS_Superblock_type *prev, *next;
};

#define VFS_ICACHE_MODULO 25
struct VFS_Superblock {
	struct VFS_Dentry *root;
	void *ctx;
	struct VFS_Superblock_type *type;
	struct VFS_Superblock *next;
	struct VFS_Inode *inodeLists[VFS_ICACHE_MODULO];
	struct VFS_Superblock *prevMounted, *nextMounted;
	struct VFS_Dentry *mountLocation;
	struct Mutex mutex;
};

void VFS_Initialize(struct VFS_Superblock *sb);

bool VFS_Dentry_MountInitializedFS(const char *path, struct VFS_Superblock *sb);
bool VFS_UserMount(const char *path, const char *devpath, const char *fs_type_name);
bool VFS_UserUnmount(const char *path);
void VFS_RegisterFilesystem(struct VFS_Superblock_type *type);

struct File *VFS_Open(const char *path, int perm);
void VFS_FinalizeFile(struct File *fd);

#endif
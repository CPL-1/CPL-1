#include <common/core/fd/fd.h>
#include <common/core/fd/vfs.h>
#include <common/core/memory/heap.h>
#include <common/core/proc/mutex.h>
#include <common/core/storage/partdev.h>
#include <common/core/storage/storage.h>

struct PartDev_InodeData {
	UINT64 start;
	UINT64 count;
	struct Storage_Device *storage;
};

static int partdev_fd_rw(struct File *file, int size, char *buf, bool write) {
	if (size == 0) {
		return 0;
	} else if (size < 0) {
		return -1;
	}
	struct PartDev_InodeData *inoData = (struct PartDev_InodeData *)(file->ctx);
	off_t endOffset = (off_t)size + file->offset;
	if (endOffset < 0) {
		return -1;
	}
	if (endOffset < file->offset || (UINT64)endOffset > inoData->count) {
		return -1;
	}
	if (!storageRW(inoData->storage, inoData->start + file->offset, size, buf, write)) {
		return -1;
	}
	return size;
}

static off_t partdev_fd_callback_lseek(struct File *file, off_t offset, int whence) {
	if (whence != SEEK_SET) {
		return -1;
	}
	if (offset < 0) {
		return -1;
	}
	struct PartDev_InodeData *inoData = (struct PartDev_InodeData *)(file->ctx);
	if ((UINT64)offset > inoData->count) {
		return -1;
	}
	return offset;
}

static int partdev_fd_callback_read(struct File *file, int size, char *buf) {
	return partdev_fd_rw(file, size, buf, false);
}

static int partdev_fd_callback_write(struct File *file, int size, const char *buf) {
	return partdev_fd_rw(file, size, (char *)buf, true);
}

static void partdev_fd_callback_flush(struct File *file) {
	struct PartDev_InodeData *ino_data = (struct PartDev_InodeData *)(file->ctx);
	storageFlush(ino_data->storage);
}

static void partdev_fd_callback_close(struct File *file) {
	struct PartDev_InodeData *ino_data = (struct PartDev_InodeData *)(file->ctx);
	struct Storage_Device *storage = ino_data->storage;
	Storage_LockClosePartition(storage);
	VFS_FinalizeFile(file);
}

static struct FileOperations partdev_fd_ops = {
	.read = partdev_fd_callback_read,
	.write = partdev_fd_callback_write,
	.readdir = NULL,
	.lseek = partdev_fd_callback_lseek,
	.flush = partdev_fd_callback_flush,
	.close = partdev_fd_callback_close,
};

static struct File *partdev_callback_open(struct VFS_Inode *inode, UNUSED int perm) {
	struct File *fd = ALLOC_OBJ(struct File);
	if (fd == NULL) {
		return NULL;
	}
	struct PartDev_InodeData *ino_data = (struct PartDev_InodeData *)(inode->ctx);
	struct Storage_Device *storage = ino_data->storage;
	if (!Storage_LockTryOpenPartition(storage)) {
		FREE_OBJ(fd);
		return NULL;
	}
	fd->ctx = (void *)ino_data;
	fd->ops = &partdev_fd_ops;
	fd->offset = 0;
	return fd;
}

static struct VFS_InodeOperations partdev_inode_ops = {
	.getChild = NULL,
	.open = partdev_callback_open,
	.mkdir = NULL,
	.link = NULL,
	.unlink = NULL,
};

struct VFS_Inode *PartDev_MakePartitionDevice(struct Storage_Device *storage, UINT64 start, UINT64 count) {
	struct VFS_Inode *partInode = ALLOC_OBJ(struct VFS_Inode);
	if (partInode == NULL) {
		return NULL;
	}
	struct PartDev_InodeData *partData = ALLOC_OBJ(struct PartDev_InodeData);
	if (partData == NULL) {
		FREE_OBJ(partInode);
		return NULL;
	}
	partData->start = start;
	partData->count = count;
	partData->storage = storage;

	partInode->stat.stBlksize = storage->sectorSize;
	partInode->stat.stBlkcnt = ALIGN_UP(count, storage->sectorSize) / storage->sectorSize;
	partInode->stat.stType = VFS_DT_BLK;
	partInode->stat.stSize = count;
	partInode->ctx = (void *)partData;
	partInode->ops = &partdev_inode_ops;
	return partInode;
}

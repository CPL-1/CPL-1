#include <common/core/fd/fd.h>
#include <common/core/fd/vfs.h>
#include <common/core/memory/heap.h>
#include <common/core/proc/mutex.h>
#include <common/core/storage/partdev.h>
#include <common/core/storage/storage.h>

struct partdev_inode_data {
	uint64_t start;
	uint64_t count;
	struct storage_dev *storage;
};

static int partdev_fd_rw(struct fd *file, int size, char *buf, bool write) {
	if (size == 0) {
		return 0;
	} else if (size < 0) {
		return -1;
	}
	struct partdev_inode_data *ino_data =
		(struct partdev_inode_data *)(file->ctx);
	off_t end_offset = (off_t)size + file->offset;
	if (end_offset < 0) {
		return -1;
	}
	if (end_offset < file->offset || (uint64_t)end_offset > ino_data->count) {
		return -1;
	}
	if (!storage_rw(ino_data->storage, ino_data->start + file->offset, size,
					buf, write)) {
		return -1;
	}
	return size;
}

static off_t partdev_fd_callback_lseek(struct fd *file, off_t offset,
									   int whence) {
	if (whence != SEEK_SET) {
		return -1;
	}
	if (offset < 0) {
		return -1;
	}
	struct partdev_inode_data *ino_data =
		(struct partdev_inode_data *)(file->ctx);
	if ((uint64_t)offset > ino_data->count) {
		return -1;
	}
	return offset;
}

static int partdev_fd_callback_read(struct fd *file, int size, char *buf) {
	return partdev_fd_rw(file, size, buf, false);
}

static int partdev_fd_callback_write(struct fd *file, int size,
									 const char *buf) {
	return partdev_fd_rw(file, size, (char *)buf, true);
}

static void partdev_fd_callback_flush(struct fd *file) {
	struct partdev_inode_data *ino_data =
		(struct partdev_inode_data *)(file->ctx);
	storage_flush(ino_data->storage);
}

static void partdev_fd_callback_close(struct fd *file) {
	struct partdev_inode_data *ino_data =
		(struct partdev_inode_data *)(file->ctx);
	struct storage_dev *storage = ino_data->storage;
	storage_lock_close_partition(storage);
	vfs_finalize(file);
}

static struct fd_ops partdev_fd_ops = {
	.read = partdev_fd_callback_read,
	.write = partdev_fd_callback_write,
	.readdir = NULL,
	.lseek = partdev_fd_callback_lseek,
	.flush = partdev_fd_callback_flush,
	.close = partdev_fd_callback_close,
};

static struct fd *partdev_callback_open(struct vfs_inode *inode,
										unused int perm) {
	struct fd *fd = ALLOC_OBJ(struct fd);
	if (fd == NULL) {
		return NULL;
	}
	struct partdev_inode_data *ino_data =
		(struct partdev_inode_data *)(inode->ctx);
	struct storage_dev *storage = ino_data->storage;
	if (!storage_lock_try_open_partition(storage)) {
		FREE_OBJ(fd);
		return NULL;
	}
	fd->ctx = (void *)ino_data;
	fd->ops = &partdev_fd_ops;
	fd->offset = 0;
	return fd;
}

static struct vfs_inode_ops partdev_inode_ops = {
	.get_child = NULL,
	.open = partdev_callback_open,
	.mkdir = NULL,
	.link = NULL,
	.unlink = NULL,
};

struct vfs_inode *partdev_make(struct storage_dev *storage, uint64_t start,
							   uint64_t count) {
	struct vfs_inode *part_inode = ALLOC_OBJ(struct vfs_inode);
	if (part_inode == NULL) {
		return NULL;
	}
	struct partdev_inode_data *part_data = ALLOC_OBJ(struct partdev_inode_data);
	if (part_data == NULL) {
		FREE_OBJ(part_inode);
		return NULL;
	}
	part_data->start = start;
	part_data->count = count;
	part_data->storage = storage;

	part_inode->stat.st_blksize = storage->sector_size;
	part_inode->stat.st_blkcnt =
		ALIGN_UP(count, storage->sector_size) / storage->sector_size;
	part_inode->stat.st_type = VFS_DT_BLK;
	part_inode->stat.st_size = count;
	part_inode->ctx = (void *)part_data;
	part_inode->ops = &partdev_inode_ops;
	return part_inode;
}

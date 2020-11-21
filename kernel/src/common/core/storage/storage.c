#include <common/core/fd/fs/devfs.h>
#include <common/core/memory/heap.h>
#include <common/core/storage/mbr.h>
#include <common/core/storage/storage.h>
#include <common/lib/kmsg.h>

bool storage_cache_init(struct storage_dev *storage) {
	mutex_init(&(storage->mutex));
	return true;
}

static bool storage_lock_try_open(struct storage_dev *storage) {
	mutex_lock(&(storage->mutex));
	if (storage->opened_mode != STORAGE_NOT_OPENED) {
		mutex_unlock(&(storage->mutex));
		return false;
	}
	storage->opened_mode = STORAGE_OPENED;
	mutex_unlock(&(storage->mutex));
	return true;
}

bool storage_lock_try_open_partition(struct storage_dev *storage) {
	mutex_lock(&(storage->mutex));
	if (storage->opened_mode == STORAGE_OPENED) {
		mutex_unlock(&(storage->mutex));
		return false;
	}
	if (storage->opened_mode == STORAGE_NOT_OPENED) {
		storage->opened_mode = STORAGE_OPENED_PARTITION;
		storage->partitions_opened_count = 1;
	} else {
		storage->partitions_opened_count++;
	}
	mutex_unlock(&(storage->mutex));
	return true;
}

static void storage_lock_close(struct storage_dev *storage) {
	mutex_lock(&(storage->mutex));
	if (storage->opened_mode != STORAGE_OPENED) {
		kmsg_err("Storage Stack Manager",
				 "Attempt to close device when storage->opened_mode is not "
				 "equal to STORAGE_OPENED");
	}
	storage->opened_mode = STORAGE_NOT_OPENED;
	mutex_unlock(&(storage->mutex));
}

void storage_lock_close_partition(struct storage_dev *storage) {
	mutex_lock(&(storage->mutex));
	if (storage->opened_mode != STORAGE_OPENED_PARTITION) {
		kmsg_err("Storage Stack Manager",
				 "Attempt to close partition when storage->opened_mode is not "
				 "equal to STORAGE_OPENED_PARTITION");
	}
	if (storage->partitions_opened_count == 0) {
		kmsg_err("Storage Stack Manager",
				 "Attempt to close partition when "
				 "storage->partitions_opened_count is zero");
	}
	storage->partitions_opened_count--;
	if (storage->partitions_opened_count == 0) {
		storage->opened_mode = STORAGE_NOT_OPENED;
	}
	mutex_unlock(&(storage->mutex));
}

static bool storage_rw_one_sector(struct storage_dev *storage, uint64_t lba,
								  size_t start, size_t end, char *buf,
								  bool write) {
	char *tmp_buf = heap_alloc(storage->sector_size);
	if (tmp_buf == NULL) {
		return false;
	}
	if (write) {
		if (start != 0 || end != storage->sector_size) {
			if (!storage->rw_lba(storage->ctx, tmp_buf, lba, 1, false)) {
				heap_free(tmp_buf, storage->sector_size);
				return false;
			}
		}
		memcpy(tmp_buf + start, buf, end - start);
	}
	bool result = storage->rw_lba(storage->ctx, tmp_buf, lba, 1, write);
	if (!result) {
		heap_free(tmp_buf, storage->sector_size);
		return false;
	}
	if (!write) {
		memcpy(buf, tmp_buf + start, end - start);
	}
	heap_free(tmp_buf, storage->sector_size);
	return true;
}

static bool storage_rw_range(struct storage_dev *storage, uint64_t lba,
							 uint64_t count, char *buf, bool write) {
	uint64_t current_offset = lba;
	while (current_offset < lba + count) {
		size_t buffer_offset = (current_offset - lba) * storage->sector_size;
		uint64_t remaining = lba - current_offset;
		if (remaining > storage->max_rw_sectors_count) {
			remaining = storage->max_rw_sectors_count;
		}
		if (!storage->rw_lba(storage->ctx, buf + buffer_offset, current_offset,
							 remaining, write)) {
			return false;
		}
		current_offset += remaining;
	}
	return true;
}

bool storage_rw(struct storage_dev *storage, uint64_t offset, size_t size,
				char *buf, bool write) {
	if ((offset + (uint64_t)size) < offset) {
		return false;
	}
	if ((offset + (uint64_t)size) >=
		storage->sector_size * storage->sectors_count) {
		return false;
	}
	mutex_lock(&(storage->mutex));
	uint64_t sector_size = (uint64_t)(storage->sector_size);
	uint64_t start_lba = offset / sector_size;
	uint64_t end_lba = (offset + size) / sector_size;
	size_t start_lba_offset = offset - start_lba * sector_size;
	size_t ending_lba_offset = (offset + size) - end_lba * sector_size;
	if (start_lba == end_lba) {
		bool result =
			storage_rw_one_sector(storage, start_lba, start_lba_offset,
								  ending_lba_offset, buf, write);
		mutex_unlock(&(storage->mutex));
		return result;
	} else if (end_lba - 1 == start_lba && ending_lba_offset == 0) {
		bool result = storage_rw_one_sector(
			storage, start_lba, start_lba_offset, sector_size, buf, write);
		mutex_unlock(&(storage->mutex));
		return result;
	} else {
		if (start_lba_offset != 0) {
			if (!storage_rw_one_sector(storage, start_lba, start_lba_offset,
									   sector_size, buf, write)) {
				mutex_unlock(&(storage->mutex));
				return false;
			}
			start_lba += 1;
		}
		if (ending_lba_offset != 0) {
			if (!storage_rw_one_sector(storage, end_lba, 0, ending_lba_offset,
									   buf + (size - ending_lba_offset),
									   write)) {
				mutex_unlock(&(storage->mutex));
				return false;
			}
		}
		bool result =
			storage_rw_range(storage, start_lba, end_lba - start_lba,
							 buf + (sector_size - start_lba_offset), write);
		mutex_unlock(&(storage->mutex));
		return result;
	}
}

void storage_flush(unused struct storage_dev *storage) {}

int storage_fd_callback_rw(struct fd *file, int size, char *buf, bool write) {
	if (size == 0) {
		return 0;
	}
	struct storage_dev *storage = (struct storage_dev *)file->ctx;
	off_t pos = file->offset;
	if (size < 1) {
		return -1;
	}
	if ((pos + (off_t)size) < pos) {
		return -1;
	}
	if ((pos + (off_t)size) >=
		(off_t)(storage->sector_size * storage->sectors_count)) {
		return -1;
	}
	if (!storage_rw(storage, (uint64_t)pos, (size_t)size, buf, write)) {
		return -1;
	}
	return size;
}

static int storage_fd_callback_read(struct fd *file, int size, char *buf) {
	return storage_fd_callback_rw(file, size, buf, false);
}

static int storage_fd_callback_write(struct fd *file, int size,
									 const char *buf) {
	return storage_fd_callback_rw(file, size, (char *)buf, true);
}

static off_t storage_fd_callback_lseek(struct fd *file, off_t offset,
									   int whence) {
	struct storage_dev *storage = (struct storage_dev *)file->ctx;
	if (whence != SEEK_SET) {
		return -1;
	}
	if (offset < 0) {
		return -1;
	}
	if (offset >= (off_t)(storage->sector_size * storage->sectors_count)) {
		return -1;
	}
	return offset;
}

static void storage_fd_callback_flush(struct fd *file) {
	struct storage_dev *storage = (struct storage_dev *)file->ctx;
	storage_flush(storage);
}

static void storage_fd_callback_close(struct fd *file) {
	struct storage_dev *storage = (struct storage_dev *)file->ctx;
	storage_flush(storage);
	storage_lock_close(storage);
	vfs_finalize(file);
}

static struct fd_ops storage_fd_ops = {.read = storage_fd_callback_read,
									   .write = storage_fd_callback_write,
									   .readdir = NULL,
									   .lseek = storage_fd_callback_lseek,
									   .flush = storage_fd_callback_flush,
									   .close = storage_fd_callback_close};

static struct fd *storage_inode_callback_open(struct vfs_inode *inode,
											  unused int perm) {
	struct fd *fd = ALLOC_OBJ(struct fd);
	if (fd == NULL) {
		return NULL;
	}
	struct storage_dev *storage = (struct storage_dev *)(inode->ctx);
	if (!storage_lock_try_open(storage)) {
		FREE_OBJ(fd);
		return NULL;
	}
	fd->ctx = inode->ctx;
	fd->ops = &storage_fd_ops;
	fd->offset = 0;
	return fd;
}

static struct vfs_inode_ops storage_inode_ops = {
	.get_child = NULL,
	.open = storage_inode_callback_open,
	.mkdir = NULL,
	.link = NULL,
	.unlink = NULL,
};

struct vfs_inode *storage_make_inode(struct storage_dev *storage) {
	struct vfs_inode *inode = ALLOC_OBJ(struct vfs_inode);
	if (inode == NULL) {
		return NULL;
	}
	inode->ctx = (void *)storage;
	inode->ops = &storage_inode_ops;
	inode->stat.st_blksize = storage->sector_size;
	inode->stat.st_blkcnt = storage->sectors_count;
	inode->stat.st_type = VFS_DT_BLK;
	inode->stat.st_size = storage->sector_size * storage->sectors_count;
	return inode;
}

bool storage_init(struct storage_dev *storage) {
	storage_cache_init(storage);
	struct vfs_inode *inode = storage_make_inode(storage);
	if (inode == NULL) {
		return false;
	}
	if (!devfs_register_inode(storage->name, inode)) {
		return false;
	}
	if (mbr_check_disk(storage)) {
		mbr_enumerate_partitions(storage);
	}
	return true;
}

void storage_make_part_name(struct storage_dev *storage, char *buf,
							unsigned int part_id) {
	if (storage->partitioning_scheme == STORAGE_NUMERIC_PART_NAMING) {
		sprintf("%s%lu\0", buf, 256, storage->name, (uint64_t)(part_id + 1));
	} else if (storage->partitioning_scheme == STORAGE_P_NUMERIC_PART_NAMING) {
		sprintf("%sp%lu\0", buf, 256, storage->name, (uint64_t)(part_id + 1));
	}
}
#include <common/core/fd/fs/devfs.h>
#include <common/core/memory/heap.h>
#include <common/core/storage/mbr.h>
#include <common/core/storage/storage.h>
#include <common/lib/kmsg.h>

bool Storage_CacheInit(struct Storage_Device *storage) {
	Mutex_Initialize(&(storage->mutex));
	return true;
}

static bool Storage_LockTryOpen(struct Storage_Device *storage) {
	Mutex_Lock(&(storage->mutex));
	if (storage->openedMode != STORAGE_NOT_OPENED) {
		Mutex_Unlock(&(storage->mutex));
		return false;
	}
	storage->openedMode = STORAGE_OPENED;
	Mutex_Unlock(&(storage->mutex));
	return true;
}

bool Storage_LockTryOpenPartition(struct Storage_Device *storage) {
	Mutex_Lock(&(storage->mutex));
	if (storage->openedMode == STORAGE_OPENED) {
		Mutex_Unlock(&(storage->mutex));
		return false;
	}
	if (storage->openedMode == STORAGE_NOT_OPENED) {
		storage->openedMode = STORAGE_OPENED_PARTITION;
		storage->partitionsOpenedCount = 1;
	} else {
		storage->partitionsOpenedCount++;
	}
	Mutex_Unlock(&(storage->mutex));
	return true;
}

static void Storage_LockClose(struct Storage_Device *storage) {
	Mutex_Lock(&(storage->mutex));
	if (storage->openedMode != STORAGE_OPENED) {
		KernelLog_ErrorMsg("Storage Stack Manager", "Attempt to close device when storage->opened_mode is not "
													"equal to STORAGE_OPENED");
	}
	storage->openedMode = STORAGE_NOT_OPENED;
	Mutex_Unlock(&(storage->mutex));
}

void Storage_LockClosePartition(struct Storage_Device *storage) {
	Mutex_Lock(&(storage->mutex));
	if (storage->openedMode != STORAGE_OPENED_PARTITION) {
		KernelLog_ErrorMsg("Storage Stack Manager", "Attempt to close partition when storage->opened_mode is not "
													"equal to STORAGE_OPENED_PARTITION");
	}
	if (storage->partitionsOpenedCount == 0) {
		KernelLog_ErrorMsg("Storage Stack Manager", "Attempt to close partition when "
													"storage->partitionsOpenedCount is zero");
	}
	storage->partitionsOpenedCount--;
	if (storage->partitionsOpenedCount == 0) {
		storage->openedMode = STORAGE_NOT_OPENED;
	}
	Mutex_Unlock(&(storage->mutex));
}

static bool StorageRWOneSector(struct Storage_Device *storage, uint64_t lba, size_t start, size_t end, char *buf,
							   bool write) {
	char *tmp_buf = Heap_AllocateMemory(storage->sectorSize);
	if (tmp_buf == NULL) {
		return false;
	}
	if (write) {
		if (start != 0 || end != storage->sectorSize) {
			if (!storage->rw_lba(storage->ctx, tmp_buf, lba, 1, false)) {
				Heap_FreeMemory(tmp_buf, storage->sectorSize);
				return false;
			}
		}
		memcpy(tmp_buf + start, buf, end - start);
	}
	bool result = storage->rw_lba(storage->ctx, tmp_buf, lba, 1, write);
	if (!result) {
		Heap_FreeMemory(tmp_buf, storage->sectorSize);
		return false;
	}
	if (!write) {
		memcpy(buf, tmp_buf + start, end - start);
	}
	Heap_FreeMemory(tmp_buf, storage->sectorSize);
	return true;
}

static bool StorageRWRange(struct Storage_Device *storage, uint64_t lba, uint64_t count, char *buf, bool write) {
	uint64_t current_offset = lba;
	while (current_offset < lba + count) {
		size_t buffer_offset = (current_offset - lba) * storage->sectorSize;
		uint64_t remaining = lba + count - current_offset;
		if (remaining > storage->maxRWSectorsCount) {
			remaining = storage->maxRWSectorsCount;
		}
		if (!storage->rw_lba(storage->ctx, buf + buffer_offset, current_offset, remaining, write)) {
			return false;
		}
		current_offset += remaining;
	}
	return true;
}

bool Storage_ReadWrite(struct Storage_Device *storage, uint64_t offset, size_t size, char *buf, bool write) {
	if ((offset + (uint64_t)size) < offset) {
		return false;
	}
	if ((offset + (uint64_t)size) >= storage->sectorSize * storage->sectorsCount) {
		return false;
	}
	Mutex_Lock(&(storage->mutex));
	uint64_t sector_size = (uint64_t)(storage->sectorSize);
	uint64_t start_lba = offset / sector_size;
	uint64_t end_lba = (offset + size) / sector_size;
	size_t start_lba_offset = offset - start_lba * sector_size;
	size_t ending_lba_offset = (offset + size) - end_lba * sector_size;
	if (start_lba == end_lba) {
		bool result = StorageRWOneSector(storage, start_lba, start_lba_offset, ending_lba_offset, buf, write);
		Mutex_Unlock(&(storage->mutex));
		return result;
	} else if (end_lba - 1 == start_lba && ending_lba_offset == 0) {
		bool result = StorageRWOneSector(storage, start_lba, start_lba_offset, sector_size, buf, write);
		Mutex_Unlock(&(storage->mutex));
		return result;
	} else {
		size_t large_zone_offset = 0;
		if (start_lba_offset != 0) {
			if (!StorageRWOneSector(storage, start_lba, start_lba_offset, sector_size, buf, write)) {
				Mutex_Unlock(&(storage->mutex));
				return false;
			}
			start_lba += 1;
			large_zone_offset = start_lba_offset;
		}
		if (ending_lba_offset != 0) {
			if (!StorageRWOneSector(storage, end_lba, 0, ending_lba_offset, buf + (size - ending_lba_offset), write)) {
				Mutex_Unlock(&(storage->mutex));
				return false;
			}
		}
		bool result = StorageRWRange(storage, start_lba, end_lba - start_lba, buf + large_zone_offset, write);
		Mutex_Unlock(&(storage->mutex));
		return result;
	}
}

void Storage_Flush(UNUSED struct Storage_Device *storage) {
}

int storageFDCallbackRW(struct File *file, int size, char *buf, bool write) {
	if (size == 0) {
		return 0;
	}
	struct Storage_Device *storage = (struct Storage_Device *)file->ctx;
	off_t pos = file->offset;
	if (size < 1) {
		return -1;
	}
	if ((pos + (off_t)size) < pos) {
		return -1;
	}
	if ((pos + (off_t)size) >= (off_t)(storage->sectorSize * storage->sectorsCount)) {
		return -1;
	}
	if (!Storage_ReadWrite(storage, (uint64_t)pos, (size_t)size, buf, write)) {
		return -1;
	}
	return size;
}

static int storageFDCallbackRead(struct File *file, int size, char *buf) {
	return storageFDCallbackRW(file, size, buf, false);
}

static int storageFDCallbackWrite(struct File *file, int size, const char *buf) {
	return storageFDCallbackRW(file, size, (char *)buf, true);
}

static off_t Storage_FDCallbackLseek(struct File *file, off_t offset, int whence) {
	struct Storage_Device *storage = (struct Storage_Device *)file->ctx;
	if (whence != SEEK_SET) {
		return -1;
	}
	if (offset < 0) {
		return -1;
	}
	if (offset >= (off_t)(storage->sectorSize * storage->sectorsCount)) {
		return -1;
	}
	return offset;
}

static void Storage_FDCallbackFlush(struct File *file) {
	struct Storage_Device *storage = (struct Storage_Device *)file->ctx;
	Storage_Flush(storage);
}

static void Storage_FDCallbackClose(struct File *file) {
	struct Storage_Device *storage = (struct Storage_Device *)file->ctx;
	Storage_Flush(storage);
	Storage_LockClose(storage);
	VFS_FinalizeFile(file);
}

static struct FileOperations m_StorageFileOperations = {.read = storageFDCallbackRead,
														.write = storageFDCallbackWrite,
														.readdir = NULL,
														.lseek = Storage_FDCallbackLseek,
														.flush = Storage_FDCallbackFlush,
														.close = Storage_FDCallbackClose};

struct File *Storage_FileOpen(struct VFS_Inode *inode, UNUSED int perm) {
	struct File *fd = ALLOC_OBJ(struct File);
	if (fd == NULL) {
		return NULL;
	}
	struct Storage_Device *storage = (struct Storage_Device *)(inode->ctx);
	if (!Storage_LockTryOpen(storage)) {
		FREE_OBJ(fd);
		return NULL;
	}
	fd->ctx = inode->ctx;
	fd->ops = &m_StorageFileOperations;
	fd->offset = 0;
	return fd;
}

static struct VFS_InodeOperations storage_inode_ops = {
	.getChild = NULL,
	.open = Storage_FileOpen,
	.mkdir = NULL,
	.link = NULL,
	.unlink = NULL,
};

struct VFS_Inode *Storage_MakeInode(struct Storage_Device *storage) {
	struct VFS_Inode *inode = ALLOC_OBJ(struct VFS_Inode);
	if (inode == NULL) {
		return NULL;
	}
	inode->ctx = (void *)storage;
	inode->ops = &storage_inode_ops;
	inode->stat.stBlksize = storage->sectorSize;
	inode->stat.stBlkcnt = storage->sectorsCount;
	inode->stat.stType = VFS_DT_BLK;
	inode->stat.stSize = storage->sectorSize * storage->sectorsCount;
	return inode;
}

bool Storage_init(struct Storage_Device *storage) {
	Storage_CacheInit(storage);
	struct VFS_Inode *inode = Storage_MakeInode(storage);
	if (inode == NULL) {
		return false;
	}
	if (!DevFS_RegisterInode(storage->name, inode)) {
		return false;
	}
	if (MBR_CheckDisk(storage)) {
		MBR_EnumeratePartitions(storage);
	}
	return true;
}

void Storage_MakePartitionName(struct Storage_Device *storage, char *buf, unsigned int part_id) {
	if (storage->partitioningScheme == STORAGE_NUMERIC_PART_NAMING) {
		sprintf("%s%lu\0", buf, 256, storage->name, (uint64_t)(part_id + 1));
	} else if (storage->partitioningScheme == STORAGE_P_NUMERIC_PART_NAMING) {
		sprintf("%sp%lu\0", buf, 256, storage->name, (uint64_t)(part_id + 1));
	}
}
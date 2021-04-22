#include <common/core/devices/storage/partdev.h>
#include <common/core/devices/storage/storage.h>
#include <common/core/fd/fd.h>
#include <common/core/fd/vfs.h>
#include <common/core/memory/heap.h>
#include <common/core/proc/mutex.h>

struct PartDev_InodeData {
	uint64_t start;
	uint64_t count;
	struct Storage_Device *storage;
};

static int PartDev_File_ReadWrite(struct File *file, int size, char *buf, bool write) {
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
	if (endOffset < file->offset || (uint64_t)endOffset > inoData->count) {
		return -1;
	}
	if (!Storage_ReadWrite(inoData->storage, inoData->start + file->offset, size, buf, write)) {
		return -1;
	}
	return size;
}

static off_t PartDev_Lseek(struct File *file, off_t offset, int whence) {
	if (whence != SEEK_SET) {
		return -1;
	}
	if (offset < 0) {
		return -1;
	}
	struct PartDev_InodeData *inoData = (struct PartDev_InodeData *)(file->ctx);
	if ((uint64_t)offset > inoData->count) {
		return -1;
	}
	return offset;
}

static int PartDev_Read(struct File *file, int size, char *buf) {
	return PartDev_File_ReadWrite(file, size, buf, false);
}

static int PartDev_Write(struct File *file, int size, const char *buf) {
	return PartDev_File_ReadWrite(file, size, (char *)buf, true);
}

static void PartDev_Flush(struct File *file) {
	struct PartDev_InodeData *ino_data = (struct PartDev_InodeData *)(file->ctx);
	Storage_Flush(ino_data->storage);
}

static void PartDev_Close(struct File *file) {
	struct PartDev_InodeData *ino_data = (struct PartDev_InodeData *)(file->ctx);
	struct Storage_Device *storage = ino_data->storage;
	Storage_LockClosePartition(storage);
	VFS_FinalizeFile(file);
}

static struct FileOperations m_partDevFileOperations = {
	.read = PartDev_Read,
	.write = PartDev_Write,
	.readdir = NULL,
	.lseek = PartDev_Lseek,
	.flush = PartDev_Flush,
	.close = PartDev_Close,
};

static struct File *PartDev_Open(struct VFS_Inode *inode, MAYBE_UNUSED int perm) {
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
	fd->ops = &m_partDevFileOperations;
	fd->offset = 0;
	return fd;
}

static struct VFS_InodeOperations m_inodeOperations = {
	.getChild = NULL,
	.open = PartDev_Open,
	.mkdir = NULL,
	.link = NULL,
	.unlink = NULL,
};

struct VFS_Inode *PartDev_MakePartitionDevice(struct Storage_Device *storage, uint64_t start, uint64_t count) {
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

	partInode->stat.st_blksize = storage->sectorSize;
	partInode->stat.st_blkcnt = ALIGN_UP(count, storage->sectorSize) / storage->sectorSize;
	partInode->stat.st_type = VFS_DT_BLK;
	partInode->stat.st_size = count;
	partInode->ctx = (void *)partData;
	partInode->ops = &m_inodeOperations;
	return partInode;
}

#ifndef __CACHE_H_INCLUDED__
#define __CACHE_H_INCLUDED__

#include <common/core/fd/vfs.h>
#include <common/core/proc/mutex.h>
#include <common/misc/utils.h>

struct Storage_Device {
	size_t sectorSize;
	size_t maxRWSectorsCount;
	uint64_t sectorsCount;
	void *ctx;
	bool (*rw_lba)(void *ctx, char *buf, uint64_t lba, size_t count, bool write);
	struct Mutex mutex;
	char name[256];
	enum
	{
		STORAGE_NUMERIC_PART_NAMING,
		STORAGE_P_NUMERIC_PART_NAMING,
		STORAGE_NO_PART_ENUMERATION,
	} partitioningScheme;
	enum
	{
		STORAGE_NOT_OPENED,
		STORAGE_OPENED_PARTITION,
		STORAGE_OPENED,
	} openedMode;
	size_t partitions_opened_count;
};

bool storageInit(struct Storage_Device *storage);
bool Storage_LockTryOpenPartition(struct Storage_Device *storage);
void Storage_LockClosePartition(struct Storage_Device *storage);
bool storageRW(struct Storage_Device *storage, uint64_t offset, size_t size, char *buf, bool write);
void storageFlush(struct Storage_Device *storage);
struct File *storageFileOpen(struct VFS_Inode *inode, int perm);
struct VFS_Inode *storageMakeInode(struct Storage_Device *storage);
void storageMakePartitionName(struct Storage_Device *storage, char *buf, unsigned int part_id);

#endif
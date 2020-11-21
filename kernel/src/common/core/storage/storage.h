#ifndef __CACHE_H_INCLUDED__
#define __CACHE_H_INCLUDED__

#include <common/core/fd/vfs.h>
#include <common/core/proc/mutex.h>
#include <common/misc/utils.h>

struct storage_dev {
	size_t sector_size;
	size_t max_rw_sectors_count;
	uint64_t sectors_count;
	void *ctx;
	bool (*rw_lba)(void *ctx, char *buf, uint64_t lba, size_t count,
				   bool write);
	struct mutex mutex;
	char name[256];
	enum {
		STORAGE_NUMERIC_PART_NAMING,
		STORAGE_P_NUMERIC_PART_NAMING,
		STORAGE_NO_PART_ENUMERATION,
	} partitioning_scheme;
	enum {
		STORAGE_NOT_OPENED,
		STORAGE_OPENED_PARTITION,
		STORAGE_OPENED,
	} opened_mode;
	size_t partitions_opened_count;
};

bool storage_init(struct storage_dev *storage);
bool storage_lock_try_open_partition(struct storage_dev *storage);
void storage_lock_close_partition(struct storage_dev *storage);
bool storage_rw(struct storage_dev *storage, uint64_t offset, size_t size,
				char *buf, bool write);
void storage_flush(struct storage_dev *storage);
struct fd *storage_fd_open(struct vfs_inode *inode, int perm);
struct vfs_inode *storage_make_inode(struct storage_dev *storage);

void storage_make_part_name(struct storage_dev *storage, char *buf,
							size_t part_id);

#endif
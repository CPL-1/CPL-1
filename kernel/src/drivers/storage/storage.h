#ifndef __CACHE_H_INCLUDED__
#define __CACHE_H_INCLUDED__

#include <fd/vfs.h>
#include <proc/mutex.h>
#include <utils.h>

struct storage {
	size_t sector_size;
	size_t max_rw_sectors_count;
	uint64_t sectors_count;
	void *ctx;
	bool (*rw_lba)(void *ctx, char *buf, uint64_t lba, size_t count,
				   bool write);
	struct mutex mutex;
};

bool storage_init(struct storage *storage);
bool storage_rw(struct storage *storage, uint64_t offset, size_t size,
				char *buf, bool write);
void storage_flush(struct storage *storage);

int storage_fd_callback_read(struct fd *file, int size, char *buf);

int storage_fd_callback_write(struct fd *file, int size, const char *buf);

off_t storage_fd_callback_lseek(struct fd *file, off_t offset, int whence);

int storage_fd_callback_close(struct fd *file);

void storage_fd_callback_flush(struct fd *file);

struct fd *storage_fd_open(struct vfs_inode *inode, int perm);

struct vfs_inode *storage_make_inode(struct storage *storage);

#endif
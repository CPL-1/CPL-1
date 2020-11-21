#ifndef __PART_DEV_H_INCLUDED__
#define __PART_DEV_H_INCLUDED__

#include <common/core/storage/storage.h>
#include <common/misc/utils.h>

struct vfs_inode *partdev_make(struct storage_dev *storage, uint64_t start,
							   uint64_t count);

#endif
#ifndef __PART_DEV_H_INCLUDED__
#define __PART_DEV_H_INCLUDED__

#include <common/core/devices/storage/storage.h>
#include <common/misc/utils.h>

struct VFS_Inode *PartDev_MakePartitionDevice(struct Storage_Device *storage, uint64_t start, uint64_t count);

#endif

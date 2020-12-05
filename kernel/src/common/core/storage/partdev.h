#ifndef __PART_DEV_H_INCLUDED__
#define __PART_DEV_H_INCLUDED__

#include <common/core/storage/storage.h>
#include <common/misc/utils.h>

struct VFS_Inode *PartDev_MakePartitionDevice(struct Storage_Device *storage, UINT64 start, UINT64 count);

#endif
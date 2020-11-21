#ifndef __MBR_H_INCLUDED__
#define __MBR_H_INCLUDED__

#include <common/core/storage/mbr.h>
#include <common/core/storage/storage.h>

bool mbr_check_disk(struct storage_dev *dev);
bool mbr_enumerate_partitions(struct storage_dev *dev);

#endif
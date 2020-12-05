#ifndef __MBR_H_INCLUDED__
#define __MBR_H_INCLUDED__

#include <common/core/storage/mbr.h>
#include <common/core/storage/storage.h>

bool MBR_CheckDisk(struct Storage_Device *dev);
bool MBR_EnumeratePartitions(struct Storage_Device *dev);

#endif
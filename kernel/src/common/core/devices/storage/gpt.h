#ifndef __GPT_H_INCLUDED__
#define __GPT_H_INCLUDED__

#include <common/core/devices/storage/gpt.h>
#include <common/core/devices/storage/storage.h>

bool GPT_CheckDisk(struct Storage_Device *dev);
bool GPT_EnumeratePartitions(struct Storage_Device *dev);

#endif

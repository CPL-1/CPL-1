#include <common/core/fd/fs/devfs.h>
#include <common/core/memory/heap.h>
#include <common/core/storage/mbr.h>
#include <common/core/storage/partdev.h>
#include <common/core/storage/storage.h>

bool MBR_CheckDisk(struct Storage_Device *dev) {
	char magic[3];
	magic[2] = '\0';
	if (!storageRW(dev, 510, 2, magic, false)) {
		return false;
	}
	return StringsEqual(magic, "\x55\xaa");
}

struct mbr_entry {
	UINT32 status : 8;
	UINT32 startHead : 8;
	UINT32 startCylinder : 10;
	UINT32 startSector : 6;
	UINT32 code : 8;
	UINT32 endHead : 8;
	UINT32 endCylinder : 10;
	UINT32 endSector : 6;
	UINT32 startingLba : 32;
	UINT32 lbaSize : 32;
} PACKED LITTLE_ENDIAN NOALIGN;

bool MBR_EnumeratePartitions(struct Storage_Device *dev) {
	struct mbr_entry entries[4];
	if (!storageRW(dev, 0x1be, sizeof(entries), (char *)entries, false)) {
		return false;
	}
	struct VFS_Inode *partdevs[4];
	for (USIZE i = 0; i < 4; ++i) {
		if (entries[i].code != 0x05 && entries[i].code != 0x0f && entries[i].code != 0x00) {
			partdevs[i] = PartDev_MakePartitionDevice(dev, (UINT64)(entries[i].startingLba) * dev->sectorSize,
													  (UINT64)(entries[i].lbaSize) * dev->sectorSize);
			if (partdevs[i] == NULL) {
				for (USIZE j = 0; j < i; ++j) {
					if (partdevs[j] != NULL) {
						FREE_OBJ(partdevs[j]);
					}
				}
				return false;
			}

		} else {
			partdevs[i] = NULL;
		}
	}
	for (USIZE i = 0; i < 4; ++i) {
		if (partdevs[i] == NULL) {
			continue;
		}
		char buf[256];
		memset(buf, 0, 256);
		storageMakePartitionName(dev, buf, i);
		if (!DevFS_RegisterInode(buf, partdevs[i])) {
			FREE_OBJ(partdevs[i]);
		}
	}
	return true;
}

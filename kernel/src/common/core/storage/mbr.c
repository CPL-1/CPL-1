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
	uint32_t status : 8;
	uint32_t startHead : 8;
	uint32_t startCylinder : 10;
	uint32_t startSector : 6;
	uint32_t code : 8;
	uint32_t endHead : 8;
	uint32_t endCylinder : 10;
	uint32_t endSector : 6;
	uint32_t startingLba : 32;
	uint32_t lbaSize : 32;
} PACKED LITTLE_ENDIAN NOALIGN;

bool MBR_EnumeratePartitions(struct Storage_Device *dev) {
	struct mbr_entry entries[4];
	if (!storageRW(dev, 0x1be, sizeof(entries), (char *)entries, false)) {
		return false;
	}
	struct VFS_Inode *partdevs[4];
	for (size_t i = 0; i < 4; ++i) {
		if (entries[i].code != 0x05 && entries[i].code != 0x0f && entries[i].code != 0x00) {
			partdevs[i] = PartDev_MakePartitionDevice(dev, (uint64_t)(entries[i].startingLba) * dev->sectorSize,
													  (uint64_t)(entries[i].lbaSize) * dev->sectorSize);
			if (partdevs[i] == NULL) {
				for (size_t j = 0; j < i; ++j) {
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
	for (size_t i = 0; i < 4; ++i) {
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

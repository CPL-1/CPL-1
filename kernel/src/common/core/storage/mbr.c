#include <common/core/fd/fs/devfs.h>
#include <common/core/memory/heap.h>
#include <common/core/storage/mbr.h>
#include <common/core/storage/partdev.h>
#include <common/core/storage/storage.h>

bool mbr_check_disk(struct storage_dev *dev) {
	char magic[3];
	magic[2] = '\0';
	if (!storage_rw(dev, 510, 2, magic, false)) {
		return false;
	}
	return streq(magic, "\x55\xaa");
}

struct mbr_entry {
	uint32_t status : 8;
	uint32_t start_head : 8;
	uint32_t start_cylinder : 10;
	uint32_t start_sector : 6;
	uint32_t code : 8;
	uint32_t end_head : 8;
	uint32_t end_cylinder : 10;
	uint32_t end_sector : 6;
	uint32_t starting_lba : 32;
	uint32_t lba_size : 32;
} packed little_endian;

bool mbr_enumerate_partitions(struct storage_dev *dev) {
	struct mbr_entry entries[4];
	if (!storage_rw(dev, 0x1be, sizeof(entries), (char *)entries, false)) {
		printf("Here (32)\n");
		return false;
	}
	struct vfs_inode *partdevs[4];
	for (size_t i = 0; i < 4; ++i) {
		if (entries[i].code != 0x05 && entries[i].code != 0x0f &&
			entries[i].code != 0x00) {
			partdevs[i] = partdev_make(
				dev, (uint64_t)(entries[i].starting_lba) * dev->sector_size,
				(uint64_t)(entries[i].lba_size) * dev->sector_size);
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
		storage_make_part_name(dev, buf, i);
		if (!devfs_register_inode(buf, partdevs[i])) {
			FREE_OBJ(partdevs[i]);
		}
	}
	return true;
}

#ifndef __STORAGE_H_INCLUDED__
#define __STORAGE_H_INCLUDED__

#include <utils.h>

#define STORAGE_INV_LBA ((uint64_t)0xffffffffffffffff)

struct storage_drive {
	uint32_t drive_number;
	void *private_info;
	uint64_t sectors_count;
	size_t sector_size;
	struct storage_drive *next;
	struct storage_drive_ops *ops;
};

struct storage_drive_ops {
	bool (*rw_sectors)(struct storage_drive *drive, uint64_t lba, uint8_t *buf,
	                   size_t count, bool write);
};

void storage_init();
void storage_add_drive(struct storage_drive *drive);
struct storage_drive *storage_get(uint32_t drive_number);
bool storage_write_bytes(struct storage_drive *drive, uint64_t offset,
                         uint64_t size, const uint8_t *buf);
bool storage_read_bytes(struct storage_drive *drive, uint64_t offset,
                        uint64_t size, uint8_t *buf);
void storage_flush();

#endif
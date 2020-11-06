#ifndef __STORAGE_H_INCLUDED__
#define __STORAGE_H_INCLUDED__

#include <utils.h>

#define STORAGE_INV_LBA ((uint64_t)0xffffffffffffffff)

typedef uint8_t storage_sector_t[512];

struct storage_drive_ops {
	bool (*read_sector)(uint64_t offset, uint8_t *buf);
	bool (*write_sector)(uint64_t offset, uint8_t *buf);
};

struct storage_drive {
	uint32_t drive_number;
	void *private_info;
	uint64_t highest_lba;
	struct storage_drive *next;
	struct storage_drive_ops *ops;
};

void storage_init();
void storage_add_drive(struct storage_drive *drive);
struct storage_drive *storage_get(uint32_t drive_number);
bool storage_write_sector(struct storage_drive *drive, uint64_t lba,
                          uint8_t *buf);
bool storage_read_sector(struct storage_drive *drive, uint64_t lba,
                         uint8_t *buf);
void storage_flush();

#endif
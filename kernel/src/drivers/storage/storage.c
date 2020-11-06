#include <drivers/storage/storage.h>
#include <kmsg.h>
#include <memory/heap.h>

static struct storage_drive *storage_drives_head;

void storage_init() { storage_drives_head = NULL; }

void storage_add_drive(struct storage_drive *drive) {
	drive->next = storage_drives_head;
	storage_drives_head = drive;
}

struct storage_drive *storage_get(uint32_t drive_number) {
	struct storage_drive *drive = storage_drives_head;
	while (drive != NULL) {
		if (drive->drive_number == drive_number) {
			return drive;
		}
		drive = drive->next;
	}
	return NULL;
}

bool storage_write_sector(struct storage_drive *drive, uint64_t lba,
                          uint8_t *buf) {
	if (lba > drive->highest_lba) {
		return false;
	}
	return drive->ops->write_sector(lba, buf);
}

bool storage_read_sector(struct storage_drive *drive, uint64_t lba,
                         uint8_t *buf) {
	if (lba > drive->highest_lba) {
		return false;
	}
	return drive->ops->read_sector(lba, buf);
}
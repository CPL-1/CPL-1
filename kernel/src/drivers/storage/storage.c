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

static bool storage_rw_bytes(struct storage_drive *drive, uint64_t offset,
                             uint64_t size, uint8_t *buf, bool write) {
	uint64_t limit = drive->sector_size * drive->sectors_count;
	uint64_t end = offset + size;
	if (!(end < limit && offset < limit)) {
		return false;
	}
	if (size == 0) {
		return true;
	}
	uint64_t aligned_area_start = ALIGN_DOWN(offset, drive->sector_size);
	uint64_t starting_lba = aligned_area_start / drive->sector_size;
	uint64_t aligned_area_end = ALIGN_UP(end, drive->sector_size);
	uint64_t aligned_area_size = aligned_area_end - aligned_area_start;
	uint64_t ending_lba = aligned_area_end / drive->sector_size;
	uint64_t sectors_count = ending_lba - starting_lba;
	uint64_t first_sector_delta = offset - aligned_area_start;
	uint64_t last_sector_delta = aligned_area_end - end;
	uint8_t *da_buffer = heap_alloc(aligned_area_size);
	if (da_buffer == NULL) {
		return false;
	}
	if (write) {
		if (first_sector_delta != 0) {
			if (!drive->ops->rw_sectors(drive, starting_lba, da_buffer, 1,
			                            false)) {
				goto fail_cleanup;
			}
		}
		if (last_sector_delta != 0 && (ending_lba - 1 != starting_lba)) {
			if (!(drive->ops->rw_sectors(
			        drive, ending_lba - 1,
			        da_buffer + aligned_area_size - drive->sector_size, 1,
			        false))) {
				goto fail_cleanup;
			}
		}
		memcpy(da_buffer + first_sector_delta, buf, size);
		if (!(drive->ops->rw_sectors(drive, starting_lba, da_buffer,
		                             sectors_count, true))) {
			goto fail_cleanup;
		}
	} else {
		if (!drive->ops->rw_sectors(drive, starting_lba, da_buffer,
		                            sectors_count, false)) {
			goto fail_cleanup;
		}
		memcpy(buf, da_buffer + first_sector_delta, size);
	}
	heap_free(da_buffer, aligned_area_size);
	return true;
fail_cleanup:
	heap_free(da_buffer, aligned_area_size);
	return false;
}

bool storage_write_bytes(struct storage_drive *drive, uint64_t offset,
                         uint64_t size, const uint8_t *buf) {
	return storage_rw_bytes(drive, offset, size, (uint8_t *)buf, true);
}

bool storage_read_bytes(struct storage_drive *drive, uint64_t offset,
                        uint64_t size, uint8_t *buf) {
	return storage_rw_bytes(drive, offset, size, buf, false);
}
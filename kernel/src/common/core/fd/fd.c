#include <common/core/fd/fd.h>
#include <common/core/memory/heap.h>

int File_Read(struct File *file, int count, char *buf) {
	if (file->ops->read == NULL) {
		return -1;
	}
	return file->ops->read(file, count, buf);
}

int File_Write(struct File *file, int count, const char *buf) {
	if (file->ops->write == NULL) {
		return -1;
	}
	return file->ops->write(file, count, buf);
}

int File_Readdir(struct File *file, struct DirectoryEntry *buf, int count) {
	if (file->ops->readdir == NULL) {
		return -1;
	}
	int result = 0;
	while (result < count) {
		int code = file->ops->readdir(file, buf + result);
		if (code == 0) {
			return result;
		} else if (code == 1) {
			++result;
			continue;
		}
		return code;
	}
	return result;
}

off_t File_Lseek(struct File *file, off_t offset, int whence) {
	if (file->ops->lseek == NULL) {
		return -1;
	}
	off_t result = file->ops->lseek(file, offset, whence);
	if (result == -1) {
		return -1;
	}
	file->offset = result;
	return result;
}

void File_Flush(struct File *file) {
	if (file->ops->flush == NULL) {
		return;
	}
	file->ops->flush(file);
}

void File_Close(struct File *file) {
	if (file->ops->close != NULL) {
		file->ops->close(file);
	}
	FREE_OBJ(file);
}

int File_Readat(struct File *file, off_t pos, int count, char *buf) {
	if (file->ops->lseek == NULL || file->ops->read == NULL) {
		return -1;
	}
	int result;
	if ((result = File_Lseek(file, pos, SEEK_SET)) < 0) {
		return result;
	}
	return File_Read(file, count, buf);
}

int File_Writeat(struct File *file, off_t pos, int count, const char *buf) {
	if (file->ops->lseek == NULL || file->ops->read == NULL) {
		return -1;
	}
	int result;
	if ((result = File_Lseek(file, pos, SEEK_SET)) < 0) {
		return result;
	}
	return File_Write(file, count, buf);
}
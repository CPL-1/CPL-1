#include <common/core/fd/fd.h>
#include <common/core/memory/heap.h>
#include <hal/memory/virt.h>

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

int File_ReadAt(struct File *file, off_t pos, int count, char *buf) {
	if (file->ops->lseek == NULL || file->ops->read == NULL) {
		return -1;
	}
	int result;
	if ((result = File_Lseek(file, pos, SEEK_SET)) < 0) {
		return result;
	}
	return File_Read(file, count, buf);
}

int File_WriteAt(struct File *file, off_t pos, int count, const char *buf) {
	if (file->ops->lseek == NULL || file->ops->write == NULL) {
		return -1;
	}
	int result;
	if ((result = File_Lseek(file, pos, SEEK_SET)) < 0) {
		return result;
	}
	return File_Write(file, count, buf);
}

int File_ReadUser(struct File *file, int count, char *buf) {
	if (file->ops->read == NULL) {
		return -1;
	}
	int read = 0;
	uintptr_t currentOffset = (uintptr_t)buf;
	while (count > 0) {
		uintptr_t pageBoundaryEnd = ALIGN_UP(currentOffset + 1, HAL_VirtualMM_PageSize);
		size_t chunkSize = pageBoundaryEnd - currentOffset;
		if (chunkSize > (size_t)count) {
			chunkSize = (size_t)count;
		}
		int blockSize = (int)chunkSize;
		int result = File_Read(file, blockSize, (char *)currentOffset);
		if (result < 0) {
			return result;
		}
		if (result != count) {
			read += result;
			return result;
		}
		read += result;
		count -= result;
		currentOffset = pageBoundaryEnd;
	}
	return read;
}

int File_WriteUser(struct File *file, int count, const char *buf) {
	if (file->ops->read == NULL) {
		return -1;
	}
	int read = 0;
	uintptr_t currentOffset = (uintptr_t)buf;
	while (count > 0) {
		uintptr_t pageBoundaryEnd = ALIGN_UP(currentOffset + 1, HAL_VirtualMM_PageSize);
		size_t chunkSize = pageBoundaryEnd - currentOffset;
		if (chunkSize > (size_t)count) {
			chunkSize = (size_t)count;
		}
		int blockSize = (int)chunkSize;
		int result = File_Write(file, blockSize, (char *)currentOffset);
		if (result < 0) {
			return result;
		}
		if (result != count) {
			read += result;
			return result;
		}
		read += result;
		count -= result;
		currentOffset = pageBoundaryEnd;
	}
	return read;
}

int File_ReadAtUser(struct File *file, off_t pos, int count, char *buf) {
	if (file->ops->lseek == NULL || file->ops->read == NULL) {
		return -1;
	}
	int result;
	if ((result = File_Lseek(file, pos, SEEK_SET)) < 0) {
		return result;
	}
	return File_ReadUser(file, count, buf);
}

int File_WriteAtUser(struct File *file, off_t pos, int count, const char *buf) {
	if (file->ops->lseek == NULL || file->ops->write == NULL) {
		return -1;
	}
	int result;
	if ((result = File_Lseek(file, pos, SEEK_SET)) < 0) {
		return result;
	}
	return File_WriteUser(file, count, buf);
}
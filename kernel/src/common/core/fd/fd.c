#include <common/core/fd/fd.h>
#include <common/core/memory/heap.h>
#include <common/lib/kmsg.h>
#include <hal/memory/virt.h>

int File_ReadWithoutLocking(struct File *file, int count, char *buf) {
	if (file->ops->read == NULL) {
		return -1;
	}
	return file->ops->read(file, count, buf);
}

int File_WriteWithoutLocking(struct File *file, int count, const char *buf) {
	if (file->ops->write == NULL) {
		return -1;
	}
	return file->ops->write(file, count, buf);
}

int File_Read(struct File *file, int count, char *buf) {
	Mutex_Lock(&(file->mutex));
	int result = File_ReadWithoutLocking(file, count, buf);
	Mutex_Unlock(&(file->mutex));
	return result;
}

int File_Write(struct File *file, int count, const char *buf) {
	Mutex_Lock(&(file->mutex));
	int result = File_WriteWithoutLocking(file, count, buf);
	Mutex_Unlock(&(file->mutex));
	return result;
}

int File_Readdir(struct File *file, struct DirectoryEntry *buf, int count) {
	if (file->ops->readdir == NULL) {
		return -1;
	}
	Mutex_Lock(&(file->mutex));
	int result = 0;
	while (result < count) {
		int code = file->ops->readdir(file, buf + result);
		if (code == 0) {
			Mutex_Unlock(&(file->mutex));
			return result;
		} else if (code == 1) {
			++result;
			continue;
		}
		Mutex_Unlock(&(file->mutex));
		return code;
	}
	Mutex_Unlock(&(file->mutex));
	return result;
}

off_t File_LseekWithoutLocking(struct File *file, off_t offset, int whence) {
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

off_t File_Lseek(struct File *file, off_t offset, int whence) {
	Mutex_Lock(&(file->mutex));
	off_t result = File_LseekWithoutLocking(file, offset, whence);
	Mutex_Unlock(&(file->mutex));
	return result;
}

void File_Flush(struct File *file) {
	if (file->ops->flush == NULL) {
		return;
	}
	Mutex_Lock(&(file->mutex));
	file->ops->flush(file);
	Mutex_Unlock(&(file->mutex));
}

void File_Ref(struct File *file) {
	Mutex_Lock(&(file->mutex));
	file->refCount++;
	Mutex_Unlock(&(file->mutex));
}

void File_Drop(struct File *file) {
	Mutex_Lock(&(file->mutex));
	if (file->refCount == 0) {
		KernelLog_ErrorMsg("File Descriptor Manager", "Attempt to close file when its refcount is zero");
	}
	file->refCount--;
	if (file->refCount != 0) {
		Mutex_Unlock(&(file->mutex));
		return;
	}
	Mutex_Unlock(&(file->mutex));
	if (file->ops->close != NULL) {
		file->ops->close(file);
	}
	FREE_OBJ(file);
}

int File_PReadWithoutLocking(struct File *file, off_t pos, int count, char *buf) {
	if (file->ops->lseek == NULL || file->ops->read == NULL) {
		return -1;
	}
	off_t origPos = file->offset;
	int result;
	if ((result = (int)File_LseekWithoutLocking(file, pos, SEEK_SET)) < 0) {
		return result;
	}
	if ((result = File_ReadWithoutLocking(file, count, buf)) < 0) {
		return result;
	}
	int secondError;
	if ((secondError = (int)File_LseekWithoutLocking(file, origPos, SEEK_SET)) < 0) {
		return secondError;
	}
	return result;
}

int File_PWriteWithoutLocking(struct File *file, off_t pos, int count, const char *buf) {
	if (file->ops->lseek == NULL || file->ops->read == NULL) {
		return -1;
	}
	off_t origPos = file->offset;
	int result;
	if ((result = (int)File_LseekWithoutLocking(file, pos, SEEK_SET)) < 0) {
		return result;
	}
	if ((result = File_WriteWithoutLocking(file, count, buf)) < 0) {
		return result;
	}
	int secondError;
	if ((secondError = (int)File_LseekWithoutLocking(file, origPos, SEEK_SET)) < 0) {
		return secondError;
	}
	return result;
}

int File_PRead(struct File *file, off_t pos, int count, char *buf) {
	Mutex_Lock(&(file->mutex));
	int result = File_PReadWithoutLocking(file, pos, count, buf);
	Mutex_Unlock(&(file->mutex));
	return result;
}

int File_PWrite(struct File *file, off_t pos, int count, const char *buf) {
	Mutex_Lock(&(file->mutex));
	int result = File_PWriteWithoutLocking(file, pos, count, buf);
	Mutex_Unlock(&(file->mutex));
	return result;
}

int File_ReadUserWithoutLocking(struct File *file, int count, char *buf) {
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
		int result = File_ReadWithoutLocking(file, blockSize, (char *)currentOffset);
		if (result < 0) {
			return result;
		}
		if (result != blockSize) {
			read += result;
			return result;
		}
		read += result;
		count -= result;
		currentOffset = pageBoundaryEnd;
	}
	return read;
}

int File_WriteUserWithoutLocking(struct File *file, int count, const char *buf) {
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
		int result = File_WriteWithoutLocking(file, blockSize, (char *)currentOffset);
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

int File_ReadUser(struct File *file, int count, char *buf) {
	Mutex_Lock(&(file->mutex));
	int result = File_ReadUserWithoutLocking(file, count, buf);
	Mutex_Unlock(&(file->mutex));
	return result;
}

int File_WriteUser(struct File *file, int count, const char *buf) {
	Mutex_Lock(&(file->mutex));
	int result = File_WriteUserWithoutLocking(file, count, buf);
	Mutex_Unlock(&(file->mutex));
	return result;
}

int File_PReadUser(struct File *file, off_t pos, int count, char *buf) {
	if (file->ops->lseek == NULL || file->ops->read == NULL) {
		return -1;
	}
	Mutex_Lock(&(file->mutex));
	off_t origPos = file->offset;
	int result;
	if ((result = (int)File_LseekWithoutLocking(file, pos, SEEK_SET)) < 0) {
		Mutex_Unlock(&(file->mutex));
		return result;
	}
	if ((result = File_ReadUserWithoutLocking(file, count, buf)) < 0) {
		Mutex_Unlock(&(file->mutex));
		return result;
	}
	int secondError;
	if ((secondError = (int)File_LseekWithoutLocking(file, origPos, SEEK_SET)) < 0) {
		Mutex_Unlock(&(file->mutex));
		return secondError;
	}
	Mutex_Unlock(&(file->mutex));
	return result;
}

int File_PWriteUser(struct File *file, off_t pos, int count, const char *buf) {
	if (file->ops->lseek == NULL || file->ops->read == NULL) {
		return -1;
	}
	Mutex_Lock(&(file->mutex));
	off_t origPos = file->offset;
	int result;
	if ((result = (int)File_LseekWithoutLocking(file, pos, SEEK_SET)) < 0) {
		Mutex_Unlock(&(file->mutex));
		return result;
	}
	if ((result = File_WriteUserWithoutLocking(file, count, buf)) < 0) {
		Mutex_Unlock(&(file->mutex));
		return result;
	}
	int secondError;
	if ((secondError = (int)File_LseekWithoutLocking(file, origPos, SEEK_SET)) < 0) {
		Mutex_Unlock(&(file->mutex));
		return secondError;
	}
	Mutex_Unlock(&(file->mutex));
	return result;
}

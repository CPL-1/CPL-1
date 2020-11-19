#include <core/fd/fd.h>
#include <core/memory/heap.h>

int fd_read(struct fd *file, int count, char *buf) {
	if (file->ops->read == NULL) {
		return -1;
	}
	return file->ops->read(file, count, buf);
}

int fd_write(struct fd *file, int count, const char *buf) {
	if (file->ops->write == NULL) {
		return -1;
	}
	return file->ops->write(file, count, buf);
}

int fd_readdir(struct fd *file, struct fd_dirent *buf, int count) {
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

off_t fd_lseek(struct fd *file, off_t offset, int whence) {
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

void fd_flush(struct fd *file) {
	if (file->ops->flush == NULL) {
		return;
	}
	file->ops->flush(file);
}

void fd_close(struct fd *file) {
	if (file->ops->close != NULL) {
		file->ops->close(file);
	}
	FREE_OBJ(file);
}

int fd_readat(struct fd *file, off_t pos, int count, char *buf) {
	if (file->ops->lseek == NULL || file->ops->read == NULL) {
		return -1;
	}
	int result;
	if ((result = fd_lseek(file, pos, SEEK_SET)) < 0) {
		return result;
	}
	return fd_read(file, count, buf);
}

int fd_writeat(struct fd *file, off_t pos, int count, const char *buf) {
	if (file->ops->lseek == NULL || file->ops->read == NULL) {
		return -1;
	}
	int result;
	if ((result = fd_lseek(file, pos, SEEK_SET)) < 0) {
		return result;
	}
	return fd_write(file, count, buf);
}
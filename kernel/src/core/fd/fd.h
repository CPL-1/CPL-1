#ifndef __FD_H_INCLUDED__
#define __FD_H_INCLUDED__

#include <core/fd/fdtypes.h>

enum { SEEK_SET = 0, SEEK_CUR = 1, SEEK_END = 2 };

struct fd {
	off_t offset;
	void *ctx;
	struct fd_ops *ops;
	struct vfs_dentry *dentry;
};

#define VFS_MAX_NAME_LENGTH 255
struct fd_dirent {
	ino_t dt_ino;
	char dt_name[VFS_MAX_NAME_LENGTH + 1];
};

struct fd_ops {
	int (*read)(struct fd *file, int size, char *buf);
	int (*write)(struct fd *file, int size, const char *buf);
	int (*readdir)(struct fd *file, struct fd_dirent *buf);
	off_t (*lseek)(struct fd *file, off_t offset, int whence);
	void (*flush)(struct fd *file);
	void (*close)(struct fd *file);
};

int fd_read(struct fd *file, int count, char *buf);
int fd_write(struct fd *file, int count, const char *buf);
int fd_readdir(struct fd *file, struct fd_dirent *buf, int count);
off_t fd_lseek(struct fd *file, off_t offset, int whence);
void fd_flush(struct fd *file);
void fd_close(struct fd *file);

int fd_readat(struct fd *file, off_t pos, int count, char *buf);
int fd_writeat(struct fd *file, off_t pos, int count, const char *buf);

#endif
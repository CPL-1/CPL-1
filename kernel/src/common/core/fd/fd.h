#ifndef __FD_H_INCLUDED__
#define __FD_H_INCLUDED__

#include <common/core/fd/fdtypes.h>

enum
{
	SEEK_SET = 0,
	SEEK_CUR = 1,
	SEEK_END = 2
};

struct File {
	off_t offset;
	void *ctx;
	struct FileOperations *ops;
	struct VFS_Dentry *dentry;
};

#define VFS_MAX_NAME_LENGTH 255
struct DirectoryEntry {
	ino_t dtIno;
	char dtName[VFS_MAX_NAME_LENGTH + 1];
};

struct FileOperations {
	int (*read)(struct File *file, int size, char *buf);
	int (*write)(struct File *file, int size, const char *buf);
	int (*readdir)(struct File *file, struct DirectoryEntry *buf);
	off_t (*lseek)(struct File *file, off_t offset, int whence);
	void (*flush)(struct File *file);
	void (*close)(struct File *file);
};

int File_Read(struct File *file, int count, char *buf);
int File_Write(struct File *file, int count, const char *buf);
int File_Readdir(struct File *file, struct DirectoryEntry *buf, int count);
off_t File_Lseek(struct File *file, off_t offset, int whence);
void File_Flush(struct File *file);
void File_Close(struct File *file);

int File_Readat(struct File *file, off_t pos, int count, char *buf);
int File_Writeat(struct File *file, off_t pos, int count, const char *buf);

#endif
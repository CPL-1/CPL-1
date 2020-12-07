#ifndef __FD_H_INCLUDED__
#define __FD_H_INCLUDED__

#include <common/core/fd/fdtypes.h>
#include <common/core/proc/mutex.h>
#include <common/core/proc/proc.h>

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
	struct Mutex mutex;
	size_t refCount;
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
int File_ReadUser(struct File *file, int count, char *buf);
int File_WriteUser(struct File *file, int count, const char *buf);

int File_Readdir(struct File *file, struct DirectoryEntry *buf, int count);
off_t File_Lseek(struct File *file, off_t offset, int whence);
void File_Flush(struct File *file);

struct File *File_Ref(struct File *file);
void File_Drop(struct File *file);

int File_PRead(struct File *file, off_t pos, int count, char *buf);
int File_PWrite(struct File *file, off_t pos, int count, const char *buf);
int File_PReadUser(struct File *file, off_t pos, int count, char *buf);
int File_PWriteUser(struct File *file, off_t pos, int count, const char *buf);

#endif
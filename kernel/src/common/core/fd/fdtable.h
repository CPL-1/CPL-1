#ifndef __FDTABLE_H_INCLUDED__
#define __FDTABLE_H_INCLUDED__

#include <common/core/fd/fd.h>

struct FileTable *FileTable_MakeNewTable();
struct FileTable *FileTable_GetProcessFileTable();
int FileTable_AllocateFileSlot(struct FileTable *table, struct File *file);
struct File *FileTable_Grab(struct FileTable *table, int fd);

int FileTable_FileRead(struct FileTable *table, int fd, char *buf, int size);
int FileTable_FileWrite(struct FileTable *table, int fd, const char *buf, int size);
int FileTable_FilePRead(struct FileTable *table, int fd, char *buf, int size, off_t offset);
int FileTable_FilePWrite(struct FileTable *table, int fd, const char *buf, int size, off_t offset);
int FileTable_IsATTY(struct FileTable *table, int fd);
off_t FileTable_FileLseek(struct FileTable *table, int fd, off_t newOffset, int whence);
int FileTable_FileReaddir(struct FileTable *table, int fd, struct DirectoryEntry *buf, int count);

int FileTable_FileClose(struct FileTable *table, int fd);
struct FileTable *FileTable_Ref(struct FileTable *table);
struct FileTable *FileTable_Fork(struct FileTable *table);
void FileTable_Drop(struct FileTable *table);

#endif
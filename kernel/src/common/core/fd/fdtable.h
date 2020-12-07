#ifndef __FDTABLE_H_INCLUDED__
#define __FDTABLE_H_INCLUDED__

#include <common/core/fd/fd.h>

struct FileTable *FileTable_MakeNewTable();
struct FileTable *FileTable_GetProcessFileTable();
int FileTable_AllocateFileSlot(struct FileTable *table, struct File *file);

int FileTable_FileRead(struct FileTable *table, int fd, char *buf, int size);
int FileTable_FileWrite(struct FileTable *table, int fd, const char *buf, int size);
int FileTable_FilePRead(struct FileTable *table, int fd, char *buf, int size, off_t offset);
int FileTable_FilePWrite(struct FileTable *table, int fd, const char *buf, int size, off_t offset);
off_t FileTable_FileLseek(struct FileTable *table, int fd, off_t newOffset, int whence);
int FileTable_Readdir(struct FileTable *table, int fd, struct DirectoryEntry *buf, int count);

int FileTable_FileClose(struct FileTable *table, int fd);
struct FileTable *FileDescritorTable_Ref(struct FileTable *table);
void FileTable_Drop(struct FileTable *table);

#endif
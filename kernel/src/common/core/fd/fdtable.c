#include <common/core/fd/fdtable.h>
#include <common/core/proc/proc.h>
#include <common/core/proc/proclayout.h>
#include <common/lib/dynarray.h>
#include <common/lib/kmsg.h>

struct FileTable {
	Dynarray(struct File *) descriptors;
	struct Mutex mutex;
	size_t refCount;
};

struct FileTable *FileTable_MakeNewTable() {
	struct FileTable *table = ALLOC_OBJ(struct FileTable);
	if (table == NULL) {
		return NULL;
	}
	Dynarray(struct File *) dynarray = DYNARRAY_NEW(struct File *);
	if (dynarray == NULL) {
		FREE_OBJ(table);
		return NULL;
	}
	table->descriptors = dynarray;
	table->refCount = 1;
	Mutex_Initialize(&(table->mutex));
	return table;
}

struct FileTable *FileTable_GetProcessFileTable() {
	struct Proc_ProcessID currentProcessID = Proc_GetProcessID();
	struct Proc_Process *currentProcessData = Proc_GetProcessData(currentProcessID);
	if (currentProcessData == NULL) {
		KernelLog_InfoMsg("File Descriptor Table Manager", "Failed to get current process data");
	}
	return currentProcessData->fdTable;
}

int FileTable_AllocateFileSlot(struct FileTable *table, struct File *file) {
	if (table == NULL) {
		table = FileTable_GetProcessFileTable();
	}
	Mutex_Lock(&(table->mutex));
	size_t pos;
	Dynarray(struct File *) newDynarray = POINTER_DYNARRAY_INSERT(table->descriptors, file, &pos);
	if (newDynarray == NULL) {
		Mutex_Unlock(&(table->mutex));
		return -1;
	}
	if (pos > INT_MAX) {
		POINTER_DYNARRAY_REMOVE(table->descriptors, pos);
		Mutex_Unlock(&(table->mutex));
		return -1;
	}
	table->descriptors = newDynarray;
	Mutex_Unlock(&(table->mutex));
	return (int)pos;
}

static bool FileTable_CheckFd(struct FileTable *table, int fd) {
	if (table == NULL) {
		table = FileTable_GetProcessFileTable();
	}
	if (fd < 0) {
		return false;
	}
	if (fd > (int)DYNARRAY_LENGTH(table->descriptors)) {
		return false;
	}
	if (table->descriptors[fd] == NULL) {
		return false;
	}
	return true;
}

int FileTable_FileRead(struct FileTable *table, int fd, char *buf, int size) {
	if (table == NULL) {
		table = FileTable_GetProcessFileTable();
	}
	Mutex_Lock(&(table->mutex));
	if (!FileTable_CheckFd(table, fd)) {
		Mutex_Unlock(&(table->mutex));
		return -1;
	}
	int result = File_Read(table->descriptors[fd], size, buf);
	Mutex_Unlock(&(table->mutex));
	return result;
}

int FileTable_FileWrite(struct FileTable *table, int fd, const char *buf, int size) {
	if (table == NULL) {
		table = FileTable_GetProcessFileTable();
	}
	Mutex_Lock(&(table->mutex));
	if (!FileTable_CheckFd(table, fd)) {
		Mutex_Unlock(&(table->mutex));
		return -1;
	}
	int result = File_Write(table->descriptors[fd], size, buf);
	Mutex_Unlock(&(table->mutex));
	return result;
}

int FileTable_FilePRead(struct FileTable *table, int fd, char *buf, int size, off_t offset) {
	if (table == NULL) {
		table = FileTable_GetProcessFileTable();
	}
	Mutex_Lock(&(table->mutex));
	if (!FileTable_CheckFd(table, fd)) {
		Mutex_Unlock(&(table->mutex));
		return -1;
	}
	int result = File_PRead(table->descriptors[fd], offset, size, buf);
	Mutex_Unlock(&(table->mutex));
	return result;
}

int FileTable_FilePWrite(struct FileTable *table, int fd, const char *buf, int size, off_t offset) {
	if (table == NULL) {
		table = FileTable_GetProcessFileTable();
	}
	Mutex_Lock(&(table->mutex));
	if (!FileTable_CheckFd(table, fd)) {
		Mutex_Unlock(&(table->mutex));
		return -1;
	}
	int result = File_PWrite(table->descriptors[fd], offset, size, buf);
	Mutex_Unlock(&(table->mutex));
	return result;
}

off_t FileTable_FileLseek(struct FileTable *table, int fd, off_t offset, int whence) {
	if (table == NULL) {
		table = FileTable_GetProcessFileTable();
	}
	Mutex_Lock(&(table->mutex));
	if (!FileTable_CheckFd(table, fd)) {
		Mutex_Unlock(&(table->mutex));
		return -1;
	}
	off_t result = File_Lseek(table->descriptors[fd], offset, whence);
	Mutex_Unlock(&(table->mutex));
	return result;
}

int FileTable_FileReaddir(struct FileTable *table, int fd, struct DirectoryEntry *buf, int count) {
	if (table == NULL) {
		table = FileTable_GetProcessFileTable();
	}
	Mutex_Lock(&(table->mutex));
	if (!FileTable_CheckFd(table, fd)) {
		Mutex_Unlock(&(table->mutex));
		return -1;
	}
	int result = File_Readdir(table->descriptors[fd], buf, count);
	Mutex_Unlock(&(table->mutex));
	return result;
}

int FileTable_FileClose(struct FileTable *table, int fd) {
	if (table == NULL) {
		table = FileTable_GetProcessFileTable();
	}
	Mutex_Lock(&(table->mutex));
	if (!FileTable_CheckFd(table, fd)) {
		Mutex_Unlock(&(table->mutex));
		return -1;
	}
	File_Drop(table->descriptors[fd]);
	table->descriptors[fd] = NULL;
	Mutex_Unlock(&(table->mutex));
	return 0;
}

struct FileTable *FileTable_Ref(struct FileTable *table) {
	if (table == NULL) {
		table = FileTable_GetProcessFileTable();
	}
	Mutex_Lock(&(table->mutex));
	table->refCount++;
	Mutex_Unlock(&(table->mutex));
	return table;
}

void FileTable_Drop(struct FileTable *table) {
	if (table == NULL) {
		table = FileTable_GetProcessFileTable();
	}
	Mutex_Lock(&(table->mutex));
	if (table->refCount == 0) {
		KernelLog_ErrorMsg("File Descriptor Table Managaer",
						   "Attempt to drop reference to file descriptor table when its reference count is already 0");
	}
	table->refCount--;
	if (table->refCount > 0) {
		Mutex_Unlock(&(table->mutex));
	}
	size_t tableLength = DYNARRAY_LENGTH(table->descriptors);
	for (size_t i = 0; i < tableLength; ++i) {
		if (table->descriptors[i] != NULL) {
			File_Drop(table->descriptors[i]);
		}
	}
	DYNARRAY_DISPOSE(table->descriptors);
	FREE_OBJ(table);
}

struct FileTable *FileTable_Fork(struct FileTable *table) {
	struct FileTable *newTable = ALLOC_OBJ(struct FileTable);
	if (newTable == NULL) {
		return NULL;
	}
	if (table == NULL) {
		table = FileTable_GetProcessFileTable();
	}
	Mutex_Lock(&(table->mutex));
	AUTO fdTableCopy = DYNARRAY_DUP(table->descriptors);
	if (fdTableCopy == NULL) {
		Mutex_Unlock(&(table->mutex));
		FREE_OBJ(newTable);
		return NULL;
	}
	newTable->descriptors = fdTableCopy;
	for (size_t i = 0; i < DYNARRAY_LENGTH(newTable->descriptors); ++i) {
		if (newTable->descriptors[i] != NULL) {
			File_Ref(newTable->descriptors[i]);
		}
	}
	Mutex_Initialize(&(newTable->mutex));
	newTable->refCount = 1;
	Mutex_Unlock(&(table->mutex));
	return newTable;
}
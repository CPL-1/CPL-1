#include <common/core/fd/fd.h>
#include <common/core/fd/fs/fat32.h>
#include <common/core/fd/vfs.h>
#include <common/core/memory/heap.h>
#include <common/core/proc/mutex.h>
#include <common/lib/dynarray.h>
#include <common/lib/kmsg.h>

static struct VFS_Superblock_type FAT32_SuperblockType;
static struct VFS_InodeOperations FAT32_FileInodeOperations;
static struct VFS_InodeOperations FAT32_DirectoryInodeOperations;
UNUSED static struct FileOperations FAT32_RegularFileOperations;
UNUSED static struct FileOperations FAT32_DirectoryFileOperations;

struct FAT32_BIOSBootRecord {
	char volumeID[3];
	char oemIdentifier[8];
	uint16_t bytesPerSector;
	uint8_t sectorsPerCluster;
	uint16_t reservedSectors;
	uint8_t fatCount;
	uint16_t directoryEntriesCount;
	uint16_t totalSectors;
	uint8_t mediaDescriptorType;
	uint16_t sectorsPerFat;
	uint16_t sectorsPerTrack;
	uint16_t headsCount;
	uint32_t hiddenSectors;
	uint32_t largeSectorCount;
} PACKED LITTLE_ENDIAN NOALIGN;

struct FAT32_ExtendedBootRecord {
	uint32_t sectorsPerFAT;
	uint16_t flags;
	uint16_t version;
	uint32_t rootDirectory;
	uint16_t fsInfoSector;
	uint16_t backupBootSector;
	char reserved[12];
	uint8_t driveNumber;
	uint8_t flagsForWindowsNT;
	uint8_t signature;
	uint32_t volumeID;
	char volumeLabel[11];
	char systemID[8];
	char bootCode[420];
	uint16_t bootableSignature;
} PACKED LITTLE_ENDIAN NOALIGN;

struct FAT32_FSInfo {
	uint32_t lead_signature;
	char reserved[480];
	uint32_t anotherSignature;
	uint32_t lastSector;
	uint32_t allocationHint;
	char reserved2[12];
	uint32_t trailSignature;
} PACKED LITTLE_ENDIAN NOALIGN;

struct FAT32_ShortDirectoryEntry {
	char name[8];
	char ext[3];
	uint8_t attrib;
	uint8_t zero;
	uint8_t tenthOfASecond;
	uint16_t creationTime;
	uint16_t creationDate;
	uint16_t lastAccessDate;
	uint16_t firstClusterHigh;
	uint16_t lastModificationTime;
	uint16_t lastModificationDate;
	uint16_t firstClusterLow;
	uint32_t fileSize;
} PACKED LITTLE_ENDIAN NOALIGN;

struct FAT32_LongDirectoryEntry {
	uint8_t ordinal;
	uint16_t ucsChars[5];
	uint8_t attributes;
	uint8_t type;
	uint8_t checksum;
	uint16_t ucsChars2[6];
	uint16_t zero2;
	uint16_t ucsChars3[2];
} PACKED LITTLE_ENDIAN NOALIGN;

struct FAT32_DirectoryEntry {
	char name[VFS_MAX_NAME_LENGTH + 1];
	uint8_t attrib;
	uint32_t firstCluster;
	uint32_t fileSize;
	size_t hash;
};

struct FAT32_Superblock {
	struct File *device;
	struct FAT32_BIOSBootRecord bpb;
	struct FAT32_ExtendedBootRecord ebp;
	struct FAT32_FSInfo fsinfo;
	Dynarray(struct VFS_Inode *) openedInodes;
	struct Mutex mutex;
	size_t clusterSize;
	size_t fatLength;
	uint64_t fatOffset;
	uint64_t clustersOffset;
};

struct FAT32_Inode {
	struct FAT32_Superblock *sb;
	uint32_t firstCluster;
	uint32_t fileSize;
	uint8_t attrib;
	Dynarray(struct FAT32_DirectoryEntry *) entries;
};

struct FAT32_RWStream {
	uint32_t offsetInCluster;
	uint32_t currentCluster;
};

struct FAT32_RegularFileContext {
	struct FAT32_RWStream stream;
};

enum
{
	FAT32_ATTR_READ_ONLY = 0x01,
	FAT32_ATTR_HIDDEN = 0x02,
	FAT32_ATTR_SYSTEM = 0x04,
	FAT32_ATTR_VOLUME_ID = 0x08,
	FAT32_ATTR_DIRECTORY = 0x10,
	FAT32_ATTR_ARCHIVE = 0x20,
	FAT32_ATTR_LONG_NAME = (FAT32_ATTR_READ_ONLY | FAT32_ATTR_HIDDEN | FAT32_ATTR_SYSTEM | FAT32_ATTR_VOLUME_ID)
};

enum
{
	FAT32_END_OF_DIRECTORY = 0x00,
	FAT32_UNUSED_ENTRY = 0xe5
};

enum
{ FAT32_LAST_LFN_ENTRY_ORDINAL_MASK = 0x40, };

static uint64_t FAT32_GetClusterOffset(struct FAT32_Superblock *fat32Superblock, uint32_t clusterID) {
	return fat32Superblock->clustersOffset + (clusterID - 2) * fat32Superblock->clusterSize;
}

static uint32_t FAT32_GetNextClusterID(struct FAT32_Superblock *fat32Superblock, uint32_t clusterID) {
	uint32_t fatOffset = fat32Superblock->fatOffset + clusterID * 4;
	struct {
		uint32_t entry;
	} PACKED LITTLE_ENDIAN NOALIGN buf;
	if (!File_Readat(fat32Superblock->device, fatOffset, 4, (char *)&buf)) {
		return 0x0FFFFFF7;
	}
	return buf.entry & 0x0FFFFFFF;
}

static bool FAT32_IsEndOfClusterChain(uint32_t clusterID) {
	return clusterID >= 0x0FFFFFF7;
}

static uint64_t FAT32_GetStreamDrivePos(struct FAT32_Superblock *fat32Superblock, struct FAT32_RWStream *stream) {
	return FAT32_GetClusterOffset(fat32Superblock, stream->currentCluster) + stream->offsetInCluster;
}

static bool FAT32_MoveStreamToNextCluster(struct FAT32_Superblock *fat32Superblock, struct FAT32_RWStream *stream) {
	uint32_t nextID = FAT32_GetNextClusterID(fat32Superblock, stream->currentCluster);
	if (FAT32_IsEndOfClusterChain(nextID)) {
		return false;
	}
	stream->offsetInCluster = 0;
	stream->currentCluster = nextID;
	return true;
}

static size_t FAT32_ReadFromStream(struct FAT32_Superblock *fat32Superblock, struct FAT32_RWStream *stream, int count,
								   char *buf) {
	size_t result = 0;
	while (count > 0) {
		size_t chunkSize = count;
		size_t remainingInChunk = fat32Superblock->clusterSize - stream->offsetInCluster;
		if (remainingInChunk == 0) {
			if (!FAT32_MoveStreamToNextCluster(fat32Superblock, stream)) {
				return result;
			}
			remainingInChunk = fat32Superblock->clusterSize - stream->offsetInCluster;
		}
		if (chunkSize > remainingInChunk) {
			chunkSize = remainingInChunk;
		}
		uint64_t streamPos = FAT32_GetStreamDrivePos(fat32Superblock, stream);
		if (!File_Readat(fat32Superblock->device, streamPos, chunkSize, buf + result)) {
			return result;
		}
		count -= chunkSize;
		result += chunkSize;
		stream->offsetInCluster += chunkSize;
	}
	return result;
}

static int64_t FAT32_SkipInStream(struct FAT32_Superblock *fat32Superblock, struct FAT32_RWStream *stream,
								  int64_t count) {
	if (count < 0) {
		return -1;
	}
	int64_t result = 0;
	while (count > 0) {
		size_t chunkSize = count;
		size_t remainingInChunk = fat32Superblock->clusterSize - stream->offsetInCluster;
		if (remainingInChunk == 0) {
			if (!FAT32_MoveStreamToNextCluster(fat32Superblock, stream)) {
				return result;
			}
			remainingInChunk = fat32Superblock->clusterSize - stream->offsetInCluster;
		}
		if (chunkSize > remainingInChunk) {
			chunkSize = remainingInChunk;
		}
		count -= chunkSize;
		result += chunkSize;
		stream->offsetInCluster += chunkSize;
	}
	return result;
}

static char FAT32_ConvertToLowercase(char c) {
	if (c >= 'A' && c <= 'Z') {
		c = c - 'A' + 'a';
	}
	return c;
}

static void FAT32_ConvertShortFilename(struct FAT32_ShortDirectoryEntry *entry, char *buf) {
	size_t size = 0;
	if (entry->name[0] == ' ' || entry->name[0] == '\0') {
		goto ext;
	}
	for (size_t i = 0; i < 8; ++i) {
		if (entry->name[i] == ' ' || entry->name[i] == '\0') {
			buf[size] = '\0';
			goto ext;
		}
		buf[size] = FAT32_ConvertToLowercase(entry->name[i]);
		size++;
	}
ext:
	if (entry->ext[0] == ' ' || entry->ext[0] == '\0') {
		buf[size] = '\0';
		return;
	}
	buf[size] = '.';
	++size;
	for (size_t i = 0; i < 3; ++i) {
		if (entry->ext[i] == ' ' || entry->ext[i] == '\0') {
			buf[size] = '\0';
			return;
		}
		buf[size] = FAT32_ConvertToLowercase(entry->ext[i]);
		size++;
	}
	buf[size] = '\0';
}

// source:
// https://github.com/benkASMi686_Ports_ReadByteullock/unicode-c/blob/master/unicode.c
static int FAT32_ConvertUCS2CharacterToUTF8(uint16_t ucs2, char *buf, size_t limit) {
	static size_t UNI_SUR_HIGH_START = 0xD800;
	static size_t UNI_SUR_LOW_END = 0xDF88;
	if (ucs2 < 0x80) {
		if (limit < 1) {
			return -1;
		}
		buf[0] = ucs2;
		return 1;
	}
	if (ucs2 < 0x800) {
		if (limit < 2) {
			return -1;
		}
		buf[0] = (ucs2 >> 6) | 0xC0;
		buf[1] = (ucs2 & 0x3F) | 0x80;
		return 2;
	}
	if (ucs2 < 0xFFFF) {
		if (limit < 3) {
			return -1;
		}
		buf[0] = ((ucs2 >> 12)) | 0xE0;
		buf[1] = ((ucs2 >> 6) & 0x3F) | 0x80;
		buf[2] = ((ucs2)&0x3F) | 0x80;
		if (ucs2 >= UNI_SUR_HIGH_START && ucs2 <= UNI_SUR_LOW_END) {
			return -1;
		}
		return 3;
	}
	return -1;
}

#define FAT32_LFN_FILL_FROM_ENTRY(name)                                                                                \
	for (size_t j = 0; j < ARR_SIZE(entries[i].name); ++j) {                                                           \
		if (entries[i].name[j] == 0) {                                                                                 \
			return pos;                                                                                                \
		}                                                                                                              \
		int delta = FAT32_ConvertUCS2CharacterToUTF8(entries[i].name[j], buf + pos, size - pos);                       \
		if (delta == -1) {                                                                                             \
			return -1;                                                                                                 \
		}                                                                                                              \
		pos += delta;                                                                                                  \
	}

static int FAT32_ConvertLongFilename(Dynarray(struct FAT32_LongDirectoryEntry) entries, char *buf, size_t size) {
	size_t pos = 0;
	for (size_t i = 0; i < DYNARRAY_LENGTH(entries); ++i) {
		FAT32_LFN_FILL_FROM_ENTRY(ucsChars);
		FAT32_LFN_FILL_FROM_ENTRY(ucsChars2);
		FAT32_LFN_FILL_FROM_ENTRY(ucsChars3);
	}
	return pos;
}

#undef FAT32_LFN_FILL_FROM_ENTRY

static enum
{
	FAT32_READ_ENTRY_READ,
	FAT32_READ_ENTRY_SKIP,
	FAT32_READ_ENTRY_END,
	FAT32_READ_ENTRY_ERROR,
} FAT32_ReadDirectoryEntryFromStream(struct FAT32_Superblock *fat32Superblock, struct FAT32_DirectoryEntry *entry,
									 struct FAT32_RWStream *stream) {
	char buf[sizeof(struct FAT32_ShortDirectoryEntry)];
	struct FAT32_ShortDirectoryEntry *asShort = (struct FAT32_ShortDirectoryEntry *)&buf;
	struct FAT32_LongDirectoryEntry *asLong = (struct FAT32_LongDirectoryEntry *)&buf;
	if (FAT32_ReadFromStream(fat32Superblock, stream, sizeof(struct FAT32_ShortDirectoryEntry), buf) !=
		sizeof(struct FAT32_ShortDirectoryEntry)) {
		return FAT32_READ_ENTRY_END;
	}
	if ((uint8_t)(asShort->name[0]) == FAT32_UNUSED_ENTRY) {
		return FAT32_READ_ENTRY_SKIP;
	}
	if ((uint8_t)(asShort->name[0]) == FAT32_END_OF_DIRECTORY) {
		return FAT32_READ_ENTRY_END;
	}
	if (asShort->attrib == FAT32_ATTR_LONG_NAME) {
		Dynarray(struct FAT32_LongDirectoryEntry) longEntries = DYNARRAY_NEW(struct FAT32_LongDirectoryEntry);
		if (longEntries == NULL) {
			return FAT32_READ_ENTRY_ERROR;
		}
		Dynarray(struct FAT32_LongDirectoryEntry) withElem = DYNARRAY_PUSH(longEntries, *asLong);
		if (withElem == NULL) {
			DYNARRAY_DISPOSE(longEntries);
			return FAT32_READ_ENTRY_ERROR;
		}
		longEntries = withElem;
		while (true) {
			if (FAT32_ReadFromStream(fat32Superblock, stream, sizeof(struct FAT32_ShortDirectoryEntry), buf) !=
				sizeof(struct FAT32_ShortDirectoryEntry)) {
				DYNARRAY_DISPOSE(longEntries);
				return FAT32_READ_ENTRY_ERROR;
			}
			if (asShort->attrib != FAT32_ATTR_LONG_NAME) {
				break;
			}
			Dynarray(struct FAT32_LongDirectoryEntry) with_elem = DYNARRAY_PUSH(longEntries, *asLong);
			if (with_elem == NULL) {
				DYNARRAY_DISPOSE(longEntries);
				return FAT32_READ_ENTRY_ERROR;
			}
			longEntries = with_elem;
		}
		for (size_t i = 0; i < DYNARRAY_LENGTH(longEntries) / 2; ++i) {
			struct FAT32_LongDirectoryEntry tmp;
			tmp = longEntries[DYNARRAY_LENGTH(longEntries) - 1 - i];
			longEntries[DYNARRAY_LENGTH(longEntries) - 1 - i] = longEntries[i];
			longEntries[i] = tmp;
		}
		for (size_t i = 0; i < DYNARRAY_LENGTH(longEntries) - 1; ++i) {
			if (longEntries[i].ordinal != i + 1) {
				DYNARRAY_DISPOSE(longEntries);
				return FAT32_READ_ENTRY_ERROR;
			}
		}
		if (longEntries[DYNARRAY_LENGTH(longEntries) - 1].ordinal !=
			(DYNARRAY_LENGTH(longEntries) | FAT32_LAST_LFN_ENTRY_ORDINAL_MASK)) {
			DYNARRAY_DISPOSE(longEntries);
			return FAT32_READ_ENTRY_ERROR;
		}
		int len = FAT32_ConvertLongFilename(longEntries, entry->name, VFS_MAX_NAME_LENGTH);
		if (len == -1) {
			DYNARRAY_DISPOSE(longEntries);
			return FAT32_READ_ENTRY_ERROR;
		}
		entry->name[len] = '\0';
	} else {
		FAT32_ConvertShortFilename(asShort, entry->name);
	}
	entry->attrib = asShort->attrib;
	entry->fileSize = asShort->fileSize;
	entry->firstCluster = (asShort->firstClusterHigh) << 16U | asShort->firstClusterLow;
	entry->hash = GetStringHash(entry->name);
	return FAT32_READ_ENTRY_READ;
}

static Dynarray(struct FAT32_DirectoryEntry *)
	FAT32_ReadDirectoryFromStream(struct FAT32_Superblock *sb, struct FAT32_RWStream *stream) {
	Dynarray(struct FAT32_DirectoryEntry *) result = DYNARRAY_NEW(struct FAT32_DirectoryEntry *);
	struct FAT32_DirectoryEntry buf;
	while (true) {
		int status = FAT32_ReadDirectoryEntryFromStream(sb, &buf, stream);
		if (status == FAT32_READ_ENTRY_ERROR) {
			DYNARRAY_DISPOSE(result);
			return NULL;
		}
		if (status == FAT32_READ_ENTRY_SKIP) {
			continue;
		}
		if (status == FAT32_READ_ENTRY_END) {
			break;
		}
		struct FAT32_DirectoryEntry *dynamicBuf = ALLOC_OBJ(struct FAT32_DirectoryEntry);
		if (dynamicBuf == NULL) {
			DYNARRAY_DISPOSE(result);
			return NULL;
		}
		memcpy(dynamicBuf, &buf, sizeof(buf));
		Dynarray(struct FAT32_DirectoryEntry *) copy = DYNARRAY_PUSH(result, dynamicBuf);
		if (copy == NULL) {
			DYNARRAY_DISPOSE(result);
			return NULL;
		}
		result = copy;
	}
	return result;
}

static bool FAT32_MakeDirectoryInode(struct FAT32_Superblock *fat32Superblock, struct FAT32_DirectoryEntry *entry,
									 struct VFS_Inode *inode) {
	struct FAT32_RWStream directoryStream;
	directoryStream.currentCluster = entry->firstCluster;
	directoryStream.offsetInCluster = 0;
	Dynarray(struct FAT32_DirectoryEntry *) entries = FAT32_ReadDirectoryFromStream(fat32Superblock, &directoryStream);
	if (entries == NULL) {
		return false;
	}
	struct FAT32_Inode *inodeContext = ALLOC_OBJ(struct FAT32_Inode);
	if (inodeContext == NULL) {
		DYNARRAY_DISPOSE(entries);
		return false;
	}
	inodeContext->sb = fat32Superblock;
	inodeContext->firstCluster = entry->firstCluster;
	inodeContext->attrib = entry->attrib;
	inodeContext->fileSize = entry->fileSize;
	inodeContext->entries = entries;
	inode->ctx = (void *)inodeContext;
	inode->ops = &FAT32_DirectoryInodeOperations;
	inode->stat.stBlkcnt = entry->fileSize / fat32Superblock->clusterSize;
	inode->stat.stBlksize = fat32Superblock->clusterSize;
	inode->stat.stSize = entry->fileSize;
	inode->stat.stType = VFS_DT_DIR;
	return true;
}

static void FAT32_CleanInode(struct VFS_Inode *inode) {
	struct FAT32_Inode *ctx = (struct FAT32_Inode *)(inode->ctx);
	if (ctx->entries != NULL) {
		DYNARRAY_DISPOSE(ctx->entries);
	}
	FREE_OBJ(ctx);
}

static ino_t FAT32_TryInsertingInode(struct FAT32_Superblock *fat32Superblock, struct VFS_Inode *inode) {
	Mutex_Lock(&(fat32Superblock->mutex));
	ino_t index;
	struct Dynarray(VFS_Inode) *new_list =
		POintER_DYNARRAY_INSERT_pointer(fat32Superblock->openedInodes, inode, &index);
	if (new_list == NULL) {
		Mutex_Unlock(&(fat32Superblock->mutex));
		return 0;
	}
	fat32Superblock->openedInodes = new_list;
	Mutex_Unlock(&(fat32Superblock->mutex));
	return index + 2;
}

static ino_t FAT32_AddDirectoryInode(struct FAT32_Superblock *fat32Superblock, struct FAT32_DirectoryEntry *entry) {
	struct VFS_Inode *inode = ALLOC_OBJ(struct VFS_Inode);
	if (inode == NULL) {
		return 0;
	}
	if (!FAT32_MakeDirectoryInode(fat32Superblock, entry, inode)) {
		FREE_OBJ(inode);
		return 0;
	}
	ino_t index = FAT32_TryInsertingInode(fat32Superblock, inode);
	if (index == 0) {
		FAT32_CleanInode(inode);
		FREE_OBJ(inode);
		return 0;
	}
	return index;
}

static ino_t FAT32_AddFileInode(struct FAT32_Superblock *fat32Superblock, struct FAT32_DirectoryEntry *entry) {
	struct VFS_Inode *inode = ALLOC_OBJ(struct VFS_Inode);
	if (inode == NULL) {
		return 0;
	}
	struct FAT32_Inode *inodeContext = ALLOC_OBJ(struct FAT32_Inode);
	if (inodeContext == NULL) {
		FREE_OBJ(inode);
		return 0;
	}
	inode->ctx = (void *)inodeContext;
	inodeContext->attrib = entry->attrib;
	inodeContext->entries = NULL;
	inodeContext->fileSize = entry->fileSize;
	inodeContext->firstCluster = entry->firstCluster;
	inodeContext->sb = fat32Superblock;
	inode->ops = &FAT32_FileInodeOperations;
	inode->stat.stBlkcnt = entry->fileSize / fat32Superblock->clusterSize;
	inode->stat.stBlksize = fat32Superblock->clusterSize;
	inode->stat.stSize = entry->fileSize;
	inode->stat.stType = VFS_DT_REG;
	ino_t index = FAT32_TryInsertingInode(fat32Superblock, inode);
	if (index == 0) {
		FAT32_CleanInode(inode);
		FREE_OBJ(inode);
		return 0;
	}
	return index;
}

static ino_t FAT32_AddInode(struct FAT32_Superblock *fat32Superblock, struct FAT32_DirectoryEntry *entry) {
	if (entry->attrib & FAT32_ATTR_DIRECTORY) {
		return FAT32_AddDirectoryInode(fat32Superblock, entry);
	}
	return FAT32_AddFileInode(fat32Superblock, entry);
}

static bool FAT32_GetInode(struct VFS_Superblock *sb, struct VFS_Inode *buf, ino_t id) {
	if (id < 1) {
		return false;
	}
	struct FAT32_Superblock *fat32Superblock = (struct FAT32_Superblock *)(sb->ctx);
	Mutex_Lock(&(fat32Superblock->mutex));
	if (id == 1) {
		struct FAT32_Inode *fat32Inode = ALLOC_OBJ(struct FAT32_Inode);
		if (fat32Inode == NULL) {
			Mutex_Unlock(&(fat32Superblock->mutex));
			return false;
		}
		struct FAT32_DirectoryEntry rootEntry;
		rootEntry.attrib = FAT32_ATTR_DIRECTORY;
		rootEntry.fileSize = 0;
		rootEntry.firstCluster = fat32Superblock->ebp.rootDirectory;
		if (!FAT32_MakeDirectoryInode(fat32Superblock, &rootEntry, buf)) {
			Mutex_Unlock(&(fat32Superblock->mutex));
			return false;
		}
		Mutex_Unlock(&(fat32Superblock->mutex));
		return true;
	} else if (id > 1 && (ino_t)id <= DYNARRAY_LENGTH(fat32Superblock->openedInodes) + 1) {
		if (fat32Superblock->openedInodes[id - 2] != NULL) {
			memcpy(buf, fat32Superblock->openedInodes[id - 2], sizeof(struct VFS_Inode));
			Mutex_Unlock(&(fat32Superblock->mutex));
			return true;
		}
	}
	Mutex_Unlock(&(fat32Superblock->mutex));
	return false;
}

static void FAT32_DropInode(UNUSED struct VFS_Superblock *sb, struct VFS_Inode *ino, UNUSED ino_t id) {
	FAT32_CleanInode(ino);
	if (id == 1) {
		return;
	}
	struct FAT32_Superblock *fat32Superblock = (struct FAT32_Superblock *)(sb->ctx);
	Mutex_Lock(&(fat32Superblock->mutex));
	fat32Superblock->openedInodes = POintER_DYNARRAY_REMOVE_POintER(fat32Superblock->openedInodes, id - 2);
	Mutex_Unlock(&(fat32Superblock->mutex));
}

static ino_t FAT32_GetDirectoryChild(struct VFS_Inode *inode, const char *name) {
	struct FAT32_Inode *inodeContext = (struct FAT32_Inode *)(inode->ctx);
	size_t hash = GetStringHash(name);
	for (size_t i = 0; i < DYNARRAY_LENGTH(inodeContext->entries); ++i) {
		if (inodeContext->entries[i] == NULL) {
			continue;
		}
		if (inodeContext->entries[i]->hash != hash) {
			continue;
		}
		if (!StringsEqual(inodeContext->entries[i]->name, name)) {
			continue;
		}
		ino_t ino = FAT32_AddInode(inodeContext->sb, inodeContext->entries[i]);
		return ino;
	}
	return 0;
}

static struct File *FAT32_OpenRegularFile(struct VFS_Inode *inode, int perm) {
	if (((perm & VFS_O_RDWR) != 0) || ((perm & VFS_O_WRONLY) != 0)) {
		return NULL;
	}
	struct File *fd = ALLOC_OBJ(struct File);
	if (fd == NULL) {
		return NULL;
	}
	fd->ops = &FAT32_RegularFileOperations;
	struct FAT32_RegularFileContext *regFileContext = ALLOC_OBJ(struct FAT32_RegularFileContext);
	if (regFileContext == NULL) {
		FREE_OBJ(fd);
		return NULL;
	}
	fd->ctx = (void *)regFileContext;
	regFileContext->stream.currentCluster = ((struct FAT32_Inode *)inode->ctx)->firstCluster;
	regFileContext->stream.offsetInCluster = 0;
	fd->offset = 0;
	return fd;
}

static struct File *FAT32_OpenDirectory(UNUSED struct VFS_Inode *inode, int perm) {
	if (((perm & VFS_O_RDWR) != 0) || ((perm & VFS_O_WRONLY) != 0)) {
		return NULL;
	}
	struct File *fd = ALLOC_OBJ(struct File);
	if (fd == NULL) {
		return NULL;
	}
	fd->ops = &FAT32_DirectoryFileOperations;
	fd->offset = 0;
	return fd;
}

static off_t FAT32_LseekInRegularFile(struct File *file, off_t offset, int whence) {
	struct FAT32_RegularFileContext *ctx = (struct FAT32_RegularFileContext *)file->ctx;
	struct FAT32_Inode *inode = (struct FAT32_Inode *)(file->dentry->inode->ctx);
	if (whence == SEEK_CUR) {
		if (file->offset + offset >= (int64_t)(inode->fileSize)) {
			return -1;
		}
		off_t result = FAT32_SkipInStream(inode->sb, &(ctx->stream), offset);
		return file->offset + result;
	} else if (whence == SEEK_SET || whence == SEEK_END) {
		if (whence == SEEK_END) {
			if (offset > 0) {
				return -1;
			}
			if (offset < -(int64_t)(inode->fileSize)) {
				return -1;
			}
			offset = (int64_t)(inode->fileSize) - offset;
		}
		if (offset >= (int64_t)(inode->fileSize)) {
			return -1;
		}
		ctx->stream.currentCluster = inode->firstCluster;
		ctx->stream.offsetInCluster = 0;
		off_t result = FAT32_SkipInStream(inode->sb, &(ctx->stream), offset);
		return result;
	}
	return -1;
}

static int FAT32_RegularFileRead(struct File *file, int size, char *buf) {
	struct FAT32_RegularFileContext *ctx = (struct FAT32_RegularFileContext *)file->ctx;
	struct FAT32_Inode *inode = (struct FAT32_Inode *)(file->dentry->inode->ctx);
	if (size < 0) {
		return -1;
	}
	if ((uint32_t)file->offset + (uint32_t)size >= inode->fileSize) {
		size = inode->fileSize - file->offset;
	}
	int result = FAT32_ReadFromStream(inode->sb, &(ctx->stream), (size_t)size, buf);
	file->offset += result;
	return result;
}

static int FAT32_ReadDirectoryEntries(struct File *file, struct DirectoryEntry *buf) {
	struct FAT32_Inode *inode = (struct FAT32_Inode *)(file->dentry->inode->ctx);
	while (true) {
		if (file->offset >= DYNARRAY_LENGTH(inode->entries)) {
			return 0;
		}
		if (inode->entries[file->offset] != NULL) {
			struct FAT32_DirectoryEntry *entry = inode->entries[file->offset];
			memcpy(buf->dtName, entry->name, 256);
			buf->dtIno = 0;
			file->offset++;
			return 1;
		}
		file->offset++;
	}
	return 0;
}

static void FAT32_CloseFile(struct File *file) {
	VFS_FinalizeFile(file);
}

static struct VFS_Superblock *FAT32_Mount(const char *device_path) {
	struct VFS_Superblock *result = ALLOC_OBJ(struct VFS_Superblock);
	if (result == NULL) {
		return NULL;
	}
	struct FAT32_Superblock *fat32Superblock = ALLOC_OBJ(struct FAT32_Superblock);
	if (fat32Superblock == NULL) {
		goto failFreeResult;
	}
	result->type = &FAT32_SuperblockType;
	result->ctx = (void *)fat32Superblock;
	struct File *device = VFS_Open(device_path, VFS_O_RDWR);
	if (device == NULL) {
		goto failFreeSuperblock;
	}
	fat32Superblock->device = device;
	if (!File_Readat(device, 0, sizeof(struct FAT32_BIOSBootRecord), (char *)&(fat32Superblock->bpb))) {
		goto failCloseDevice;
	}
	if (!File_Readat(device, sizeof(struct FAT32_BIOSBootRecord), sizeof(struct FAT32_ExtendedBootRecord),
					 (char *)&(fat32Superblock->ebp))) {
		goto failCloseDevice;
	}
	uint64_t fsinfo_offset = fat32Superblock->bpb.bytesPerSector * fat32Superblock->ebp.fsInfoSector;
	if (!File_Readat(device, fsinfo_offset, sizeof(struct FAT32_FSInfo), (char *)&(fat32Superblock->fsinfo))) {
		goto failCloseDevice;
	}
	if (fat32Superblock->ebp.bootableSignature != 0xaa55) {
		goto failCloseDevice;
	} else if (fat32Superblock->fsinfo.lead_signature != 0x41615252) {
		goto failCloseDevice;
	} else if (fat32Superblock->fsinfo.anotherSignature != 0x61417272) {
		goto failCloseDevice;
	} else if (fat32Superblock->fsinfo.trailSignature != 0xAA550000) {
		goto failCloseDevice;
	}
	fat32Superblock->fatLength = (fat32Superblock->ebp.sectorsPerFAT * fat32Superblock->bpb.bytesPerSector) / 4;
	fat32Superblock->clusterSize = fat32Superblock->bpb.bytesPerSector * fat32Superblock->bpb.sectorsPerCluster;
	fat32Superblock->fatOffset = fat32Superblock->bpb.reservedSectors * fat32Superblock->bpb.bytesPerSector;
	fat32Superblock->clustersOffset = fat32Superblock->fatOffset + fat32Superblock->ebp.sectorsPerFAT *
																	   fat32Superblock->bpb.bytesPerSector *
																	   fat32Superblock->bpb.fatCount;
	fat32Superblock->openedInodes = DYNARRAY_NEW(struct VFS_Inode *);
	if (fat32Superblock->openedInodes == NULL) {
		goto failCloseDevice;
	}
	Mutex_Initialize(&(fat32Superblock->mutex));
	return result;
failCloseDevice:
	File_Close(device);
failFreeSuperblock:
	FREE_OBJ(fat32Superblock);
failFreeResult:
	FREE_OBJ(result);
	return NULL;
}

void FAT32_Unmount(struct VFS_Superblock *sb) {
	struct FAT32_Superblock *fat32Superblock = (struct FAT32_Superblock *)(sb->ctx);
	File_Close(fat32Superblock->device);
	DYNARRAY_DISPOSE(fat32Superblock->openedInodes);
}

void FAT32_Initialize() {
	FAT32_DirectoryFileOperations.close = FAT32_CloseFile;
	FAT32_DirectoryFileOperations.flush = NULL;
	FAT32_DirectoryFileOperations.lseek = NULL;
	FAT32_DirectoryFileOperations.write = NULL;
	FAT32_DirectoryFileOperations.readdir = FAT32_ReadDirectoryEntries;

	FAT32_RegularFileOperations.close = FAT32_CloseFile;
	FAT32_DirectoryFileOperations.flush = NULL;
	FAT32_RegularFileOperations.write = NULL;
	FAT32_RegularFileOperations.lseek = FAT32_LseekInRegularFile;
	FAT32_RegularFileOperations.read = FAT32_RegularFileRead;

	FAT32_DirectoryInodeOperations.getChild = FAT32_GetDirectoryChild;
	FAT32_DirectoryInodeOperations.link = NULL;
	FAT32_DirectoryInodeOperations.mkdir = NULL;
	FAT32_DirectoryInodeOperations.open = FAT32_OpenDirectory;
	FAT32_DirectoryInodeOperations.unlink = NULL;

	FAT32_FileInodeOperations.getChild = NULL;
	FAT32_FileInodeOperations.link = NULL;
	FAT32_FileInodeOperations.mkdir = NULL;
	FAT32_FileInodeOperations.open = FAT32_OpenRegularFile;
	FAT32_FileInodeOperations.unlink = NULL;

	FAT32_SuperblockType.dropInode = NULL;
	memset(FAT32_SuperblockType.fsName, 0, VFS_MAX_NAME_LENGTH);
	memcpy(FAT32_SuperblockType.fsName, "fat32", 6);
	FAT32_SuperblockType.fsNameHash = GetStringHash("fat32");
	FAT32_SuperblockType.getInode = FAT32_GetInode;
	FAT32_SuperblockType.getRootInode = NULL;
	FAT32_SuperblockType.sync = NULL;
	FAT32_SuperblockType.dropInode = FAT32_DropInode;
	FAT32_SuperblockType.umount = NULL;
	FAT32_SuperblockType.mount = FAT32_Mount;
	VFS_RegisterFilesystem(&FAT32_SuperblockType);
};
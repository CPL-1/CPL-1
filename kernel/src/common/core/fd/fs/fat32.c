#include <common/core/fd/fd.h>
#include <common/core/fd/fs/fat32.h>
#include <common/core/fd/vfs.h>
#include <common/core/memory/heap.h>
#include <common/core/proc/mutex.h>
#include <common/lib/dynarray.h>
#include <common/lib/kmsg.h>

static struct vfs_superblock_type fat32_superblock_type;
static struct vfs_inode_ops fat32_file_inode_ops;
static struct vfs_inode_ops fat32_dir_inode_ops;
unused static struct fd_ops fat32_reg_file_ops;
unused static struct fd_ops fat32_dir_file_ops;

struct fat32_bpb {
	char volume_id[3];
	char oem_identifier[8];
	uint16_t bytes_per_sector;
	uint8_t sectors_per_cluster;
	uint16_t reserved_sectors;
	uint8_t fat_count;
	uint16_t directory_entries_count;
	uint16_t total_sectors;
	uint8_t media_descriptor_type;
	uint16_t sectors_per_fat;
	uint16_t sectors_per_track;
	uint16_t heads_count;
	uint32_t hidden_sectors;
	uint32_t large_sector_count;
} packed little_endian noalign;

struct fat32_ebp {
	uint32_t sectors_per_fat;
	uint16_t flags;
	uint16_t version;
	uint32_t root_directory;
	uint16_t fs_info_sector;
	uint16_t backup_boot_sector;
	char reserved[12];
	uint8_t drive_number;
	uint8_t flags_for_win_nt;
	uint8_t signature;
	uint32_t volume_id;
	char volume_label[11];
	char system_id[8];
	char boot_code[420];
	uint16_t bootable_signature;
} packed little_endian noalign;

struct fat32_fsinfo {
	uint32_t lead_signature;
	char reserved[480];
	uint32_t another_signature;
	uint32_t last_sector;
	uint32_t allocation_hint;
	char reserved2[12];
	uint32_t trail_signature;
} packed little_endian noalign;

struct fat32_short_directory_entry {
	char name[8];
	char ext[3];
	uint8_t attrib;
	uint8_t zero;
	uint8_t tenth_of_a_second;
	uint16_t creation_time;
	uint16_t creation_date;
	uint16_t last_access_date;
	uint16_t first_cluster_hi;
	uint16_t last_mod_time;
	uint16_t last_mod_date;
	uint16_t first_cluster_lo;
	uint32_t file_size;
} packed little_endian noalign;

struct fat32_long_directory_entry {
	uint8_t ordinal;
	uint16_t ucs_chars[5];
	uint8_t attributes;
	uint8_t type;
	uint8_t checksum;
	uint16_t ucs_chars2[6];
	uint16_t zero2;
	uint16_t ucs_chars3[2];
} packed little_endian noalign;

struct fat32_directory_entry {
	char name[VFS_MAX_NAME_LENGTH + 1];
	uint8_t attrib;
	uint32_t first_cluster;
	uint32_t file_size;
	size_t hash;
};

struct fat32_superblock {
	struct fd *device;
	struct fat32_bpb bpb;
	struct fat32_ebp ebp;
	struct fat32_fsinfo fsinfo;
	dynarray(struct vfs_inode *) opened_inodes;
	struct mutex mutex;
	size_t cluster_size;
	size_t fat_length;
	uint64_t fat_offset;
	uint64_t clusters_offset;
	uint32_t *fat;
};

struct fat32_inode {
	struct fat32_superblock *sb;
	uint32_t first_cluster;
	uint32_t file_size;
	uint8_t attrib;
	dynarray(struct fat32_directory_entry *) entries;
};

struct fat32_rw_stream {
	uint32_t offset_in_cluster;
	uint32_t current_cluster;
};

struct fat32_reg_file_ctx {
	struct fat32_rw_stream stream;
};

enum {
	FAT32_ATTR_READ_ONLY = 0x01,
	FAT32_ATTR_HIDDEN = 0x02,
	FAT32_ATTR_SYSTEM = 0x04,
	FAT32_ATTR_VOLUME_ID = 0x08,
	FAT32_ATTR_DIRECTORY = 0x10,
	FAT32_ATTR_ARCHIVE = 0x20,
	FAT32_ATTR_LONG_NAME = (FAT32_ATTR_READ_ONLY | FAT32_ATTR_HIDDEN |
							FAT32_ATTR_SYSTEM | FAT32_ATTR_VOLUME_ID)
};

enum { FAT32_END_OF_DIRECTORY = 0x00, FAT32_UNUSED_ENTRY = 0xe5 };

enum {
	FAT32_LAST_LFN_ENTRY_ORDINAL_MASK = 0x40,
};

static uint64_t fat32_get_cluster_offset(struct fat32_superblock *fat32_sb,
										 uint32_t cluster_id) {
	return fat32_sb->clusters_offset +
		   (cluster_id - 2) * fat32_sb->cluster_size;
}

static uint32_t fat32_next_cluster_id(struct fat32_superblock *fat32_sb,
									  uint32_t cluster_id) {
	return fat32_sb->fat[cluster_id] & 0x0FFFFFFF;
}

static bool fat32_is_end_of_cluster_chain(uint32_t cluster_id) {
	return cluster_id >= 0x0FFFFFF7;
}

static uint64_t fat32_get_stream_pos(struct fat32_superblock *fat32_sb,
									 struct fat32_rw_stream *stream) {
	return fat32_get_cluster_offset(fat32_sb, stream->current_cluster) +
		   stream->offset_in_cluster;
}

static bool fat32_stream_move_to_next(struct fat32_superblock *fat32_sb,
									  struct fat32_rw_stream *stream) {
	uint32_t next_id = fat32_next_cluster_id(fat32_sb, stream->current_cluster);
	if (fat32_is_end_of_cluster_chain(next_id)) {
		return false;
	}
	stream->offset_in_cluster = 0;
	stream->current_cluster = next_id;
	return true;
}

static size_t fat32_read_stream(struct fat32_superblock *fat32_sb,
								struct fat32_rw_stream *stream, int count,
								char *buf) {
	size_t result = 0;
	while (count > 0) {
		size_t chunk_size = count;
		size_t remaining_in_chunk =
			fat32_sb->cluster_size - stream->offset_in_cluster;
		if (remaining_in_chunk == 0) {
			if (!fat32_stream_move_to_next(fat32_sb, stream)) {
				return result;
			}
			remaining_in_chunk =
				fat32_sb->cluster_size - stream->offset_in_cluster;
		}
		if (chunk_size > remaining_in_chunk) {
			chunk_size = remaining_in_chunk;
		}
		uint64_t stream_pos = fat32_get_stream_pos(fat32_sb, stream);
		if (!fd_readat(fat32_sb->device, stream_pos, chunk_size,
					   buf + result)) {
			return result;
		}
		count -= chunk_size;
		result += chunk_size;
		stream->offset_in_cluster += chunk_size;
	}
	return result;
}

static int64_t fat32_skip_in_stream(struct fat32_superblock *fat32_sb,
									struct fat32_rw_stream *stream,
									int64_t count) {
	if (count < 0) {
		return -1;
	}
	int64_t result = 0;
	while (count > 0) {
		size_t chunk_size = count;
		size_t remaining_in_chunk =
			fat32_sb->cluster_size - stream->offset_in_cluster;
		if (remaining_in_chunk == 0) {
			if (!fat32_stream_move_to_next(fat32_sb, stream)) {
				return result;
			}
			remaining_in_chunk =
				fat32_sb->cluster_size - stream->offset_in_cluster;
		}
		if (chunk_size > remaining_in_chunk) {
			chunk_size = remaining_in_chunk;
		}
		count -= chunk_size;
		result += chunk_size;
		stream->offset_in_cluster += chunk_size;
	}
	return result;
}

static char fat32_to_lowercase(char c) {
	if (c >= 'A' && c <= 'Z') {
		c = c - 'A' + 'a';
	}
	return c;
}

static void
fat32_convert_short_filename(struct fat32_short_directory_entry *entry,
							 char *buf) {
	size_t size = 0;
	if (entry->name[0] == ' ' || entry->name[0] == '\0') {
		goto ext;
	}
	for (size_t i = 0; i < 8; ++i) {
		if (entry->name[i] == ' ' || entry->name[i] == '\0') {
			buf[size] = '\0';
			goto ext;
		}
		buf[size] = fat32_to_lowercase(entry->name[i]);
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
		buf[size] = fat32_to_lowercase(entry->ext[i]);
		size++;
	}
	buf[size] = '\0';
}

// source: https://github.com/benkasminbullock/unicode-c/blob/master/unicode.c
static int fat32_ucs2_to_utf8(uint16_t ucs2, char *buf, size_t limit) {
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

#define FAT32_LFN_FILL_FROM_ENTRY(name)                                        \
	for (size_t j = 0; j < ARR_SIZE(entries[i].name); ++j) {                   \
		if (entries[i].name[j] == 0) {                                         \
			return pos;                                                        \
		}                                                                      \
		int delta =                                                            \
			fat32_ucs2_to_utf8(entries[i].name[j], buf + pos, size - pos);     \
		if (delta == -1) {                                                     \
			return -1;                                                         \
		}                                                                      \
		pos += delta;                                                          \
	}

static int
fat32_convert_long_filename(dynarray(struct fat32_long_directory_entry) entries,
							char *buf, size_t size) {
	size_t pos = 0;
	for (size_t i = 0; i < dynarray_len(entries); ++i) {
		FAT32_LFN_FILL_FROM_ENTRY(ucs_chars);
		FAT32_LFN_FILL_FROM_ENTRY(ucs_chars2);
		FAT32_LFN_FILL_FROM_ENTRY(ucs_chars3);
	}
	return pos;
}

#undef FAT32_LFN_FILL_FROM_ENTRY

static enum {
	FAT32_READ_ENTRY_READ,
	FAT32_READ_ENTRY_SKIP,
	FAT32_READ_ENTRY_END,
	FAT32_READ_ENTRY_ERROR,
} fat32_read_directory_entry(struct fat32_superblock *fat32_sb,
							 struct fat32_directory_entry *entry,
							 struct fat32_rw_stream *stream) {
	char buf[sizeof(struct fat32_short_directory_entry)];
	struct fat32_short_directory_entry *as_short =
		(struct fat32_short_directory_entry *)&buf;
	struct fat32_long_directory_entry *as_long =
		(struct fat32_long_directory_entry *)&buf;
	if (fat32_read_stream(fat32_sb, stream,
						  sizeof(struct fat32_short_directory_entry),
						  buf) != sizeof(struct fat32_short_directory_entry)) {
		return FAT32_READ_ENTRY_END;
	}
	if ((uint8_t)(as_short->name[0]) == FAT32_UNUSED_ENTRY) {
		return FAT32_READ_ENTRY_SKIP;
	}
	if ((uint8_t)(as_short->name[0]) == FAT32_END_OF_DIRECTORY) {
		return FAT32_READ_ENTRY_END;
	}
	if (as_short->attrib == FAT32_ATTR_LONG_NAME) {
		dynarray(struct fat32_long_directory_entry) long_entries =
			dynarray_make(struct fat32_long_directory_entry);
		if (long_entries == NULL) {
			return FAT32_READ_ENTRY_ERROR;
		}
		dynarray(struct fat32_long_directory_entry) with_elem =
			dynarray_push(long_entries, *as_long);
		if (with_elem == NULL) {
			dynarray_dispose(long_entries);
			return FAT32_READ_ENTRY_ERROR;
		}
		long_entries = with_elem;
		while (true) {
			if (fat32_read_stream(fat32_sb, stream,
								  sizeof(struct fat32_short_directory_entry),
								  buf) !=
				sizeof(struct fat32_short_directory_entry)) {
				dynarray_dispose(long_entries);
				return FAT32_READ_ENTRY_ERROR;
			}
			if (as_short->attrib != FAT32_ATTR_LONG_NAME) {
				break;
			}
			dynarray(struct fat32_long_directory_entry) with_elem =
				dynarray_push(long_entries, *as_long);
			if (with_elem == NULL) {
				dynarray_dispose(long_entries);
				return FAT32_READ_ENTRY_ERROR;
			}
			long_entries = with_elem;
		}
		for (size_t i = 0; i < dynarray_len(long_entries) / 2; ++i) {
			struct fat32_long_directory_entry tmp;
			tmp = long_entries[dynarray_len(long_entries) - 1 - i];
			long_entries[dynarray_len(long_entries) - 1 - i] = long_entries[i];
			long_entries[i] = tmp;
		}
		for (size_t i = 0; i < dynarray_len(long_entries) - 1; ++i) {
			if (long_entries[i].ordinal != i + 1) {
				dynarray_dispose(long_entries);
				return FAT32_READ_ENTRY_ERROR;
			}
		}
		if (long_entries[dynarray_len(long_entries) - 1].ordinal !=
			(dynarray_len(long_entries) | FAT32_LAST_LFN_ENTRY_ORDINAL_MASK)) {
			dynarray_dispose(long_entries);
			return FAT32_READ_ENTRY_ERROR;
		}
		int len = fat32_convert_long_filename(long_entries, entry->name,
											  VFS_MAX_NAME_LENGTH);
		if (len == -1) {
			dynarray_dispose(long_entries);
			return FAT32_READ_ENTRY_ERROR;
		}
		entry->name[len] = '\0';
	} else {
		fat32_convert_short_filename(as_short, entry->name);
	}
	entry->attrib = as_short->attrib;
	entry->file_size = as_short->file_size;
	entry->first_cluster =
		(as_short->first_cluster_hi) << 16U | as_short->first_cluster_lo;
	entry->hash = strhash(entry->name);
	return FAT32_READ_ENTRY_READ;
}

static dynarray(struct fat32_directory_entry *)
	fat32_read_directory(struct fat32_superblock *sb,
						 struct fat32_rw_stream *stream) {
	dynarray(struct fat32_directory_entry *) result =
		dynarray_make(struct fat32_directory_entry *);
	struct fat32_directory_entry buf;
	while (true) {
		int status = fat32_read_directory_entry(sb, &buf, stream);
		if (status == FAT32_READ_ENTRY_ERROR) {
			dynarray_dispose(result);
			return NULL;
		}
		if (status == FAT32_READ_ENTRY_SKIP) {
			continue;
		}
		if (status == FAT32_READ_ENTRY_END) {
			break;
		}
		struct fat32_directory_entry *dynamic_buf =
			ALLOC_OBJ(struct fat32_directory_entry);
		if (dynamic_buf == NULL) {
			dynarray_dispose(result);
			return NULL;
		}
		memcpy(dynamic_buf, &buf, sizeof(buf));
		dynarray(struct fat32_directory_entry *) copy =
			dynarray_push(result, dynamic_buf);
		if (copy == NULL) {
			dynarray_dispose(result);
			return NULL;
		}
		result = copy;
	}
	return result;
}

static bool fat32_make_directory_inode(struct fat32_superblock *fat32_sb,
									   struct fat32_directory_entry *entry,
									   struct vfs_inode *inode) {
	struct fat32_rw_stream directory_stream;
	directory_stream.current_cluster = entry->first_cluster;
	directory_stream.offset_in_cluster = 0;
	dynarray(struct fat32_directory_entry *) entries =
		fat32_read_directory(fat32_sb, &directory_stream);
	if (entries == NULL) {
		return false;
	}
	struct fat32_inode *inode_ctx = ALLOC_OBJ(struct fat32_inode);
	if (inode_ctx == NULL) {
		dynarray_dispose(entries);
		return false;
	}
	inode_ctx->sb = fat32_sb;
	inode_ctx->first_cluster = entry->first_cluster;
	inode_ctx->attrib = entry->attrib;
	inode_ctx->file_size = entry->file_size;
	inode_ctx->entries = entries;
	inode->ctx = (void *)inode_ctx;
	inode->ops = &fat32_dir_inode_ops;
	inode->stat.st_blkcnt = entry->file_size / fat32_sb->cluster_size;
	inode->stat.st_blksize = fat32_sb->cluster_size;
	inode->stat.st_size = entry->file_size;
	inode->stat.st_type = VFS_DT_DIR;
	return true;
}

static void fat32_clean_inode(struct vfs_inode *inode) {
	struct fat32_inode *ctx = (struct fat32_inode *)(inode->ctx);
	if (ctx->entries != NULL) {
		dynarray_dispose(ctx->entries);
	}
	FREE_OBJ(ctx);
}

static ino_t fat32_try_inode_insert(struct fat32_superblock *fat32_sb,
									struct vfs_inode *inode) {
	mutex_lock(&(fat32_sb->mutex));
	ino_t index;
	struct dynarray(vfs_inode) *new_list =
		pdynarray_insert_pointer(fat32_sb->opened_inodes, inode, &index);
	if (new_list == NULL) {
		mutex_unlock(&(fat32_sb->mutex));
		return 0;
	}
	fat32_sb->opened_inodes = new_list;
	mutex_unlock(&(fat32_sb->mutex));
	return index + 2;
}

static ino_t fat32_add_directory_inode(struct fat32_superblock *fat32_sb,
									   struct fat32_directory_entry *entry) {
	struct vfs_inode *inode = ALLOC_OBJ(struct vfs_inode);
	if (inode == NULL) {
		return 0;
	}
	if (!fat32_make_directory_inode(fat32_sb, entry, inode)) {
		FREE_OBJ(inode);
		return 0;
	}
	ino_t index = fat32_try_inode_insert(fat32_sb, inode);
	if (index == 0) {
		fat32_clean_inode(inode);
		FREE_OBJ(inode);
		return 0;
	}
	return index;
}

static ino_t fat32_add_file_inode(struct fat32_superblock *fat32_sb,
								  struct fat32_directory_entry *entry) {
	struct vfs_inode *inode = ALLOC_OBJ(struct vfs_inode);
	if (inode == NULL) {
		return 0;
	}
	struct fat32_inode *inode_ctx = ALLOC_OBJ(struct fat32_inode);
	if (inode_ctx == NULL) {
		FREE_OBJ(inode);
		return 0;
	}
	inode->ctx = (void *)inode_ctx;
	inode_ctx->attrib = entry->attrib;
	inode_ctx->entries = NULL;
	inode_ctx->file_size = entry->file_size;
	inode_ctx->first_cluster = entry->first_cluster;
	inode_ctx->sb = fat32_sb;
	inode->ops = &fat32_file_inode_ops;
	inode->stat.st_blkcnt = entry->file_size / fat32_sb->cluster_size;
	inode->stat.st_blksize = fat32_sb->cluster_size;
	inode->stat.st_size = entry->file_size;
	inode->stat.st_type = VFS_DT_REG;
	ino_t index = fat32_try_inode_insert(fat32_sb, inode);
	if (index == 0) {
		fat32_clean_inode(inode);
		FREE_OBJ(inode);
		return 0;
	}
	return index;
}

static ino_t fat32_add_inode(struct fat32_superblock *fat32_sb,
							 struct fat32_directory_entry *entry) {
	if (entry->attrib & FAT32_ATTR_DIRECTORY) {
		return fat32_add_directory_inode(fat32_sb, entry);
	}
	return fat32_add_file_inode(fat32_sb, entry);
}

static bool fat32_get_inode(struct vfs_superblock *sb, struct vfs_inode *buf,
							ino_t id) {
	if (id < 1) {
		return false;
	}
	struct fat32_superblock *fat32_sb = (struct fat32_superblock *)(sb->ctx);
	mutex_lock(&(fat32_sb->mutex));
	if (id == 1) {
		struct fat32_inode *fat32_ino = ALLOC_OBJ(struct fat32_inode);
		if (fat32_ino == NULL) {
			mutex_unlock(&(fat32_sb->mutex));
			return false;
		}
		struct fat32_directory_entry root_entry;
		root_entry.attrib = FAT32_ATTR_DIRECTORY;
		root_entry.file_size = 0;
		root_entry.first_cluster = fat32_sb->ebp.root_directory;
		if (!fat32_make_directory_inode(fat32_sb, &root_entry, buf)) {
			mutex_unlock(&(fat32_sb->mutex));
			return false;
		}
		mutex_unlock(&(fat32_sb->mutex));
		return true;
	} else if (id > 1 &&
			   (ino_t)id <= dynarray_len(fat32_sb->opened_inodes) + 1) {
		if (fat32_sb->opened_inodes[id - 2] != NULL) {
			memcpy(buf, fat32_sb->opened_inodes[id - 2],
				   sizeof(struct vfs_inode));
			mutex_unlock(&(fat32_sb->mutex));
			return true;
		}
	}
	mutex_unlock(&(fat32_sb->mutex));
	return false;
}

static void fat32_drop_inode(unused struct vfs_superblock *sb,
							 struct vfs_inode *ino, unused ino_t id) {
	fat32_clean_inode(ino);
	if (id == 1) {
		return;
	}
	struct fat32_superblock *fat32_sb = (struct fat32_superblock *)(sb->ctx);
	mutex_lock(&(fat32_sb->mutex));
	fat32_sb->opened_inodes =
		pdynarray_remove_pointer(fat32_sb->opened_inodes, id - 2);
	mutex_unlock(&(fat32_sb->mutex));
}

static ino_t fat32_dir_get_child(struct vfs_inode *inode, const char *name) {
	struct fat32_inode *inode_ctx = (struct fat32_inode *)(inode->ctx);
	size_t hash = strhash(name);
	for (size_t i = 0; i < dynarray_len(inode_ctx->entries); ++i) {
		if (inode_ctx->entries[i] == NULL) {
			continue;
		}
		if (inode_ctx->entries[i]->hash != hash) {
			continue;
		}
		if (!streq(inode_ctx->entries[i]->name, name)) {
			continue;
		}
		ino_t ino = fat32_add_inode(inode_ctx->sb, inode_ctx->entries[i]);
		return ino;
	}
	return 0;
}

static struct fd *fat32_open_reg_file(struct vfs_inode *inode, int perm) {
	if (((perm & VFS_O_RDWR) != 0) || ((perm & VFS_O_WRONLY) != 0)) {
		return NULL;
	}
	struct fd *fd = ALLOC_OBJ(struct fd);
	if (fd == NULL) {
		return NULL;
	}
	fd->ops = &fat32_reg_file_ops;
	struct fat32_reg_file_ctx *reg_file_ctx =
		ALLOC_OBJ(struct fat32_reg_file_ctx);
	if (reg_file_ctx == NULL) {
		FREE_OBJ(fd);
		return NULL;
	}
	fd->ctx = (void *)reg_file_ctx;
	reg_file_ctx->stream.current_cluster =
		((struct fat32_inode *)inode->ctx)->first_cluster;
	reg_file_ctx->stream.offset_in_cluster = 0;
	fd->offset = 0;
	return fd;
}

static struct fd *fat32_open_dir_file(unused struct vfs_inode *inode,
									  int perm) {
	if (((perm & VFS_O_RDWR) != 0) || ((perm & VFS_O_WRONLY) != 0)) {
		return NULL;
	}
	struct fd *fd = ALLOC_OBJ(struct fd);
	if (fd == NULL) {
		return NULL;
	}
	fd->ops = &fat32_dir_file_ops;
	fd->offset = 0;
	return fd;
}

static off_t fat32_reg_file_lseek(struct fd *file, off_t offset, int whence) {
	struct fat32_reg_file_ctx *ctx = (struct fat32_reg_file_ctx *)file->ctx;
	struct fat32_inode *inode =
		(struct fat32_inode *)(file->dentry->inode->ctx);
	if (whence == SEEK_CUR) {
		if (file->offset + offset >= (int64_t)(inode->file_size)) {
			return -1;
		}
		off_t result = fat32_skip_in_stream(inode->sb, &(ctx->stream), offset);
		return file->offset + result;
	} else if (whence == SEEK_SET || whence == SEEK_END) {
		if (whence == SEEK_END) {
			if (offset > 0) {
				return -1;
			}
			if (offset < -(int64_t)(inode->file_size)) {
				return -1;
			}
			offset = (int64_t)(inode->file_size) - offset;
		}
		if (offset >= (int64_t)(inode->file_size)) {
			return -1;
		}
		ctx->stream.current_cluster = inode->first_cluster;
		ctx->stream.offset_in_cluster = 0;
		off_t result = fat32_skip_in_stream(inode->sb, &(ctx->stream), offset);
		return result;
	}
	return -1;
}

static int fat32_reg_file_read(struct fd *file, int size, char *buf) {
	struct fat32_reg_file_ctx *ctx = (struct fat32_reg_file_ctx *)file->ctx;
	struct fat32_inode *inode =
		(struct fat32_inode *)(file->dentry->inode->ctx);
	if (size < 0) {
		return -1;
	}
	if ((uint32_t)file->offset + (uint32_t)size >= inode->file_size) {
		size = inode->file_size - file->offset;
	}
	int result =
		fat32_read_stream(inode->sb, &(ctx->stream), (size_t)size, buf);
	file->offset += result;
	return result;
}

static int fat32_dir_file_readdir(struct fd *file, struct fd_dirent *buf) {
	struct fat32_inode *inode =
		(struct fat32_inode *)(file->dentry->inode->ctx);
	while (true) {
		if (file->offset >= dynarray_len(inode->entries)) {
			return 0;
		}
		if (inode->entries[file->offset] != NULL) {
			struct fat32_directory_entry *entry = inode->entries[file->offset];
			memcpy(buf->dt_name, entry->name, 256);
			buf->dt_ino = 0;
			file->offset++;
			return 1;
		}
		file->offset++;
	}
	return 0;
}

static void fat32_close_file(struct fd *file) { vfs_finalize(file); }

static struct vfs_superblock *fat32_mount(const char *device_path) {
	struct vfs_superblock *result = ALLOC_OBJ(struct vfs_superblock);
	if (result == NULL) {
		return NULL;
	}
	struct fat32_superblock *fat32_sb = ALLOC_OBJ(struct fat32_superblock);
	if (fat32_sb == NULL) {
		goto fail_free_result;
	}
	result->type = &fat32_superblock_type;
	result->ctx = (void *)fat32_sb;
	struct fd *device = vfs_open(device_path, VFS_O_RDWR);
	if (device == NULL) {
		goto fail_free_sb;
	}
	fat32_sb->device = device;
	if (!fd_readat(device, 0, sizeof(struct fat32_bpb),
				   (char *)&(fat32_sb->bpb))) {
		goto fail_close_device;
	}
	if (!fd_readat(device, sizeof(struct fat32_bpb), sizeof(struct fat32_ebp),
				   (char *)&(fat32_sb->ebp))) {
		goto fail_close_device;
	}
	uint64_t fsinfo_offset =
		fat32_sb->bpb.bytes_per_sector * fat32_sb->ebp.fs_info_sector;
	if (!fd_readat(device, fsinfo_offset, sizeof(struct fat32_fsinfo),
				   (char *)&(fat32_sb->fsinfo))) {
		goto fail_close_device;
	}
	if (fat32_sb->ebp.bootable_signature != 0xaa55) {
		goto fail_close_device;
	} else if (fat32_sb->fsinfo.lead_signature != 0x41615252) {
		goto fail_close_device;
	} else if (fat32_sb->fsinfo.another_signature != 0x61417272) {
		goto fail_close_device;
	} else if (fat32_sb->fsinfo.trail_signature != 0xAA550000) {
		goto fail_close_device;
	}
	fat32_sb->fat_length =
		(fat32_sb->ebp.sectors_per_fat * fat32_sb->bpb.bytes_per_sector) / 4;
	fat32_sb->cluster_size =
		fat32_sb->bpb.bytes_per_sector * fat32_sb->bpb.sectors_per_cluster;
	fat32_sb->fat_offset =
		fat32_sb->bpb.reserved_sectors * fat32_sb->bpb.bytes_per_sector;
	fat32_sb->fat = (uint32_t *)heap_alloc(fat32_sb->fat_length * 4);
	fat32_sb->clusters_offset =
		fat32_sb->fat_offset + fat32_sb->ebp.sectors_per_fat *
								   fat32_sb->bpb.bytes_per_sector *
								   fat32_sb->bpb.fat_count;
	if (fat32_sb->fat == NULL) {
		goto fail_close_device;
	}
	if (!fd_readat(device, fat32_sb->fat_offset, fat32_sb->fat_length * 4,
				   (char *)(fat32_sb->fat))) {
		goto fail_free_fat;
	}
	fat32_sb->opened_inodes = dynarray_make(struct vfs_inode *);
	if (fat32_sb->opened_inodes == NULL) {
		goto fail_free_fat;
	}
	mutex_init(&(fat32_sb->mutex));
	return result;
fail_free_fat:
	FREE_OBJ(fat32_sb->fat);
fail_close_device:
	fd_close(device);
fail_free_sb:
	FREE_OBJ(fat32_sb);
fail_free_result:
	FREE_OBJ(result);
	return NULL;
}

void fat32_init() {
	fat32_dir_file_ops.close = fat32_close_file;
	fat32_dir_file_ops.flush = NULL;
	fat32_dir_file_ops.lseek = NULL;
	fat32_dir_file_ops.write = NULL;
	fat32_dir_file_ops.readdir = fat32_dir_file_readdir;

	fat32_reg_file_ops.close = fat32_close_file;
	fat32_dir_file_ops.flush = NULL;
	fat32_reg_file_ops.write = NULL;
	fat32_reg_file_ops.lseek = fat32_reg_file_lseek;
	fat32_reg_file_ops.read = fat32_reg_file_read;

	fat32_dir_inode_ops.get_child = fat32_dir_get_child;
	fat32_dir_inode_ops.link = NULL;
	fat32_dir_inode_ops.mkdir = NULL;
	fat32_dir_inode_ops.open = fat32_open_dir_file;
	fat32_dir_inode_ops.unlink = NULL;

	fat32_file_inode_ops.get_child = NULL;
	fat32_file_inode_ops.link = NULL;
	fat32_file_inode_ops.mkdir = NULL;
	fat32_file_inode_ops.open = fat32_open_reg_file;
	fat32_file_inode_ops.unlink = NULL;

	fat32_superblock_type.drop_inode = NULL;
	memset(fat32_superblock_type.fs_name, 0, VFS_MAX_NAME_LENGTH);
	memcpy(fat32_superblock_type.fs_name, "fat32", 6);
	fat32_superblock_type.fs_name_hash = strhash("fat32");
	fat32_superblock_type.get_inode = fat32_get_inode;
	fat32_superblock_type.get_root_inode = NULL;
	fat32_superblock_type.sync = NULL;
	fat32_superblock_type.drop_inode = fat32_drop_inode;
	fat32_superblock_type.umount = NULL;
	fat32_superblock_type.mount = fat32_mount;
	vfs_register_filesystem(&fat32_superblock_type);
};
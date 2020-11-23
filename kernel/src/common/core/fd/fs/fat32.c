#include <common/core/fd/fd.h>
#include <common/core/fd/fs/fat32.h>
#include <common/core/fd/vfs.h>
#include <common/core/memory/heap.h>
#include <common/core/proc/mutex.h>
#include <common/lib/dynarray.h>
#include <common/lib/kmsg.h>

static struct vfs_superblock_type fat32_superblock_type;
static struct vfs_inode_ops fat32_inode_ops;
unused static struct fd_ops fat32_file_ops;
unused static struct fd_ops fat32_dir_ops;

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
} packed little_endian;

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
} packed little_endian;

struct fat32_fsinfo {
	uint32_t lead_signature;
	char reserved[480];
	uint32_t another_signature;
	uint32_t last_sector;
	uint32_t allocation_hint;
	char reserved2[12];
	uint32_t trail_signature;
} packed little_endian;

struct fat32_superblock {
	struct fd *device;
	struct fat32_bpb bpb;
	struct fat32_ebp ebp;
	struct fat32_fsinfo fsinfo;
	dynarray(struct vfs_inode *) fat32_opened_inodes;
	size_t cluster_size;
};

struct fat32_inode {
	struct fat32_superblock *sb;
	uint64_t disk_offset;
	bool is_root;
};

static bool fat32_get_inode(struct vfs_superblock *sb, struct vfs_inode *buf,
							ino_t id) {
	struct fat32_superblock *fat32_sb = (struct fat32_superblock *)(sb->ctx);
	if (id == 1) {
		struct fat32_inode *fat32_ino = ALLOC_OBJ(struct fat32_inode);
		if (fat32_ino == NULL) {
			return NULL;
		}
		fat32_ino->is_root = true;
		fat32_ino->disk_offset = fat32_sb->ebp.root_directory;
		fat32_ino->sb = fat32_sb;
		buf->ctx = (void *)fat32_ino;
		buf->stat.st_blkcnt = 0;
		buf->stat.st_blksize = fat32_sb->bpb.bytes_per_sector;
		buf->ops = &fat32_inode_ops;
		return true;
	} else {
	}
	return false;
}

static void fat32_drop_inode(unused struct vfs_superblock *sb,
							 struct vfs_inode *ino, unused ino_t id) {
	FREE_OBJ(ino->ctx);
}

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
	fat32_sb->cluster_size =
		fat32_sb->bpb.bytes_per_sector * fat32_sb->bpb.sectors_per_cluster;
	return result;
fail_close_device:
	fd_close(device);
fail_free_sb:
	FREE_OBJ(fat32_sb);
fail_free_result:
	FREE_OBJ(result);
	return NULL;
}

void fat32_init() {
	fat32_superblock_type.drop_inode = NULL;
	memset(fat32_superblock_type.fs_name, 0, 255);
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
#include <common/core/fd/fs/devfs.h>
#include <common/core/proc/mutex.h>
#include <common/lib/dynarray.h>
#include <common/lib/kmsg.h>

struct devfs_root_dir_entry {
	const char *name;
	size_t hash;
	struct vfs_inode *inode;
};

static dynarray(struct devfs_root_dir_entry) devfs_root_entries;
static struct vfs_inode devfs_root_inode;
static struct fd_ops devfs_root_iterator_ops;
static struct vfs_inode_ops devfs_root_inode_ops;
static struct mutex devfs_mutex;
static struct vfs_superblock_type devfs_superblock_type;

static struct vfs_superblock *devfs_mount(unused const char *device_path) {
	struct vfs_superblock *sb = ALLOC_OBJ(struct vfs_superblock);
	sb->ctx = NULL;
	sb->type = &devfs_superblock_type;
	return sb;
}

static bool devfs_get_inode(unused struct vfs_superblock *sb,
							struct vfs_inode *buf, ino_t id) {
	mutex_lock(&devfs_mutex);
	if (id == 1) {
		mutex_unlock(&devfs_mutex);
		memcpy(buf, &devfs_root_inode, sizeof(*buf));
		return true;
	} else if (id > 1 && id < (ino_t)(2 + dynarray_len(devfs_root_entries))) {
		if (devfs_root_entries[id - 2].inode == NULL) {
			mutex_unlock(&devfs_mutex);
			return false;
		}
		memcpy(buf, devfs_root_entries[id - 2].inode, sizeof(*buf));
		mutex_unlock(&devfs_mutex);
		return true;
	}
	mutex_unlock(&devfs_mutex);
	return false;
}

bool devfs_register_inode(const char *name, struct vfs_inode *inode) {
	size_t name_len = strlen(name);
	if (name_len > VFS_MAX_NAME_LENGTH) {
		return false;
	}
	char *copy = heap_alloc(name_len + 1);
	if (copy == NULL) {
		return false;
	}
	memcpy(copy, name, name_len);
	copy[name_len] = '\0';

	struct devfs_root_dir_entry entry;
	entry.name = (const char *)copy;
	entry.hash = strhash(entry.name);
	entry.inode = inode;
	mutex_lock(&devfs_mutex);

	dynarray(struct devfs_root_dir_entry) new_dynarray =
		dynarray_push(devfs_root_entries, entry);

	if (new_dynarray == NULL) {
		heap_free(copy, name_len + 1);
		mutex_unlock(&devfs_mutex);
		return false;
	}
	devfs_root_entries = new_dynarray;
	devfs_root_inode.stat.st_size = dynarray_len(devfs_root_entries);
	mutex_unlock(&devfs_mutex);
	return true;
}

static int devfs_root_get_child(unused struct vfs_inode *inode,
								const char *name) {
	size_t hash = strhash(name);
	mutex_lock(&devfs_mutex);
	for (size_t i = 0; i < dynarray_len(devfs_root_entries); ++i) {
		if (devfs_root_entries[i].hash != hash) {
			continue;
		}
		if (!streq(devfs_root_entries[i].name, name)) {
			continue;
		}
		mutex_unlock(&devfs_mutex);
		return i + 2;
	}
	mutex_unlock(&devfs_mutex);
	return 0;
}

static struct fd *devfs_open_root(unused struct vfs_inode *inode,
								  unused int perm) {
	struct fd *new_file = ALLOC_OBJ(struct fd);
	if (new_file == NULL) {
		return NULL;
	}
	new_file->offset = 0;
	new_file->ctx = NULL;
	new_file->ops = &devfs_root_iterator_ops;
	return new_file;
}

static int devfs_root_iterator_readdir(struct fd *file, struct fd_dirent *buf) {
	mutex_lock(&devfs_mutex);
	off_t index = file->offset;
	if (index >= dynarray_len(devfs_root_entries)) {
		mutex_unlock(&devfs_mutex);
		return 0;
	};
	buf->dt_ino = index + 2;
	memcpy(buf->dt_name, devfs_root_entries[index].name,
		   strlen(devfs_root_entries[index].name));
	file->offset++;
	mutex_unlock(&devfs_mutex);
	return 1;
}

static void devfs_root_iterator_close(struct fd *file) { vfs_finalize(file); }

void devfs_init() {
	devfs_root_entries = dynarray_make(struct devfs_root_dir_entry);
	if (devfs_root_entries == NULL) {
		kmsg_err("Device Filesystem (devfs)",
				 "Failed to allocate dynarrays for devfs usage");
	}
	mutex_init(&devfs_mutex);

	memcpy(&(devfs_superblock_type.fs_name), "devfs", 6);
	devfs_superblock_type.fs_name_hash = strhash("devfs");
	devfs_superblock_type.get_inode = devfs_get_inode;
	devfs_superblock_type.drop_inode = NULL;
	devfs_superblock_type.sync = NULL;
	devfs_superblock_type.mount = devfs_mount;
	devfs_superblock_type.umount = NULL;
	devfs_superblock_type.get_root_inode = NULL;

	devfs_root_inode.ctx = NULL;
	devfs_root_inode.stat.st_size = 0;
	devfs_root_inode.stat.st_type = VFS_DT_DIR;
	devfs_root_inode.stat.st_blksize = 0;
	devfs_root_inode.stat.st_blkcnt = 0;
	devfs_root_inode.ops = &devfs_root_inode_ops;

	devfs_root_inode_ops.get_child = devfs_root_get_child;
	devfs_root_inode_ops.open = devfs_open_root;
	devfs_root_inode_ops.mkdir = NULL;
	devfs_root_inode_ops.link = NULL;
	devfs_root_inode_ops.unlink = NULL;

	devfs_root_iterator_ops.read = NULL;
	devfs_root_iterator_ops.write = NULL;
	devfs_root_iterator_ops.lseek = NULL;
	devfs_root_iterator_ops.flush = NULL;
	devfs_root_iterator_ops.close = devfs_root_iterator_close;
	devfs_root_iterator_ops.readdir = devfs_root_iterator_readdir;

	vfs_register_filesystem(&devfs_superblock_type);
}
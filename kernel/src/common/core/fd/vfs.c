#include <common/core/fd/vfs.h>
#include <common/core/memory/heap.h>
#include <common/lib/kmsg.h>
#include <common/lib/pathsplit.h>

static struct mutex vfs_mutex;
static struct vfs_superblock *vfs_root;
static struct vfs_superblock *vfs_sb_list;
static struct vfs_superblock_type *vfs_sb_types;

struct vfs_inode *vfs_get_inode(struct vfs_superblock *sb, ino_t id) {
	if (sb->type->get_inode == NULL) {
		return NULL;
	}
	mutex_lock(&(sb->mutex));
	ino_t modulo = id % VFS_ICACHE_MODULO;
	struct vfs_inode *current = sb->inode_lists[modulo];
	while (current != NULL) {
		if (current->id == id) {
			current->ref_count++;
			mutex_unlock(&(sb->mutex));
			return current;
		}
		current = current->next_in_cache;
	}
	struct vfs_inode *node = ALLOC_OBJ(struct vfs_inode);
	if (node == NULL) {
		mutex_unlock(&(sb->mutex));
		return NULL;
	}
	if (!sb->type->get_inode(sb, node, id)) {
		FREE_OBJ(node);
		mutex_unlock(&(sb->mutex));
		return NULL;
	}
	node->next_in_cache = sb->inode_lists[modulo];
	if (sb->inode_lists[modulo] != NULL) {
		sb->inode_lists[modulo]->prev_in_cache = node;
	}
	node->prev_in_cache = NULL;
	sb->inode_lists[modulo] = node;
	node->id = id;
	node->ref_count = 1;
	node->dirty = false;
	node->sb = sb;
	node->mount = NULL;
	mutex_init(&(node->mutex));
	mutex_unlock(&(sb->mutex));
	return node;
}

void vfs_drop_inode(struct vfs_superblock *sb, struct vfs_inode *inode) {
	mutex_lock(&(sb->mutex));
	ino_t modulo = inode->id % VFS_ICACHE_MODULO;
	if (inode->ref_count == 0) {
		kmsg_err(
			"Virtual File System",
			"How the hell we are dropping inode more times than we opened it?");
	}
	inode->ref_count--;
	if (inode->ref_count == 0) {
		if (inode->prev_in_cache == NULL) {
			sb->inode_lists[modulo] = inode->next_in_cache;
		} else {
			inode->prev_in_cache->next_in_cache = inode->next_in_cache;
		}
		if (inode->next_in_cache != NULL) {
			inode->next_in_cache->prev_in_cache = NULL;
		}
		if (sb->type->drop_inode != NULL) {
			sb->type->drop_inode(sb, inode, inode->id);
		}
		FREE_OBJ(inode);
	}
	mutex_unlock(&(sb->mutex));
}

struct vfs_inode *vfs_sb_next_inode(struct vfs_superblock *sb,
									struct vfs_inode *inode) {
	if (inode == NULL) {
		for (ino_t i = 0; i < VFS_ICACHE_MODULO; ++i) {
			if (sb->inode_lists[i] != NULL) {
				return sb->inode_lists[i];
			}
		}
	} else {
		if (inode->next_in_cache != NULL) {
			return inode->next_in_cache;
		}
		ino_t modulo = inode->id % VFS_ICACHE_MODULO;
		for (ino_t i = modulo; i < VFS_ICACHE_MODULO; ++i) {
			return sb->inode_lists[i];
		}
	}
	return NULL;
}

bool vfs_is_inode_loaded(struct vfs_superblock *sb, ino_t id) {
	ino_t modulo = id % VFS_ICACHE_MODULO;
	struct vfs_inode *current = sb->inode_lists[modulo];
	while (current != NULL) {
		if (current->id == id) {
			return true;
		}
		current = current->next_in_cache;
	}
	return false;
}

bool vfs_drop_mount(struct vfs_dentry *dentry) {
	struct vfs_superblock *sb = dentry->inode->sb;
	struct vfs_dentry *mount_location = sb->mount_location;
	size_t a = 0;
	if ((a = ATOMIC_DECREMENT(&(dentry->ref_count))) > 1) {
		mutex_unlock(&(dentry->mutex));
		return false;
	}
	mount_location->inode->mount = NULL;
	sb->mount_location = NULL;
	mutex_unlock(&(dentry->mutex));
	if (sb->type->umount != NULL) {
		sb->type->umount(sb->ctx);
	}
	mutex_lock(&vfs_mutex);
	if (sb->next_mounted != NULL) {
		sb->next_mounted->prev_mounted = sb->prev_mounted;
	}
	if (sb->prev_mounted != NULL) {
		sb->prev_mounted->next_mounted = sb->next_mounted;
	} else {
		vfs_sb_list = sb->next_mounted;
	}
	mutex_unlock(&vfs_mutex);
	FREE_OBJ(sb);
	return true;
}

struct vfs_dentry *vfs_backtrace_mounts(struct vfs_dentry *dentry) {
	while (dentry->parent == NULL) {
		struct vfs_superblock *sb = dentry->inode->sb;
		struct vfs_dentry *mount_location = sb->mount_location;
		if (sb->mount_location == NULL) {
			return dentry;
		}
		ATOMIC_INCREMENT(&(mount_location->ref_count));
		mutex_lock(&(mount_location->mutex));
		vfs_drop_mount(dentry);
		dentry = mount_location;
	}
	return dentry;
}

void vfs_dentry_cut(struct vfs_dentry *dentry) {
	struct vfs_dentry *prev = dentry->prev;
	struct vfs_dentry *next = dentry->next;
	struct vfs_dentry *par = dentry->parent;
	if (prev == NULL) {
		par->head = next;
	} else {
		prev->next = next;
	}
	if (next != NULL) {
		next->prev = prev;
	}
}

bool vfs_drop_dentry(struct vfs_dentry *dentry) {
	if (ATOMIC_DECREMENT(&(dentry->ref_count)) > 1) {
		mutex_unlock(&(dentry->mutex));
		return false;
	}
	vfs_dentry_cut(dentry);
	vfs_drop_inode(dentry->inode->sb, dentry->inode);
	ATOMIC_DECREMENT(&(dentry->parent->ref_count));
	mutex_unlock(&(dentry->mutex));
	FREE_OBJ(dentry);
	return true;
}

struct vfs_dentry *vfs_dentry_walk_to_parent(struct vfs_dentry *dentry) {
	struct vfs_dentry *backtraced = vfs_backtrace_mounts(dentry);
	if (backtraced->parent == NULL) {
		return dentry;
	}
	struct vfs_dentry *parent = backtraced->parent;
	ATOMIC_INCREMENT(&(parent->ref_count));
	mutex_lock(&(parent->mutex));
	vfs_drop_dentry(backtraced);
	return parent;
}

struct vfs_dentry *vfs_dentry_lookup(struct vfs_dentry *dentry,
									 const char *name) {
	size_t hash = strhash(name);
	struct vfs_dentry *current = dentry->head;
	while (current != NULL) {
		if (current->hash != hash) {
			continue;
		}
		if (streq(current->name, name)) {
			return current;
		}
		current = current->next;
	}
	return NULL;
}

struct vfs_dentry *vfs_dentry_load_child(struct vfs_dentry *dentry,
										 const char *name) {
	size_t name_length = strlen(name);
	if (name_length > 255) {
		return NULL;
	}
	struct vfs_superblock *sb = dentry->inode->sb;
	if (dentry->inode->ops->get_child == NULL) {
		return NULL;
	}
	ino_t id = dentry->inode->ops->get_child(dentry->inode, name);
	if (id == 0) {
		return NULL;
	}
	struct vfs_inode *new_inode = vfs_get_inode(sb, id);
	if (new_inode == NULL) {
		return NULL;
	}
	struct vfs_dentry *new_dentry = ALLOC_OBJ(struct vfs_dentry);
	if (dentry == NULL) {
		return NULL;
	}
	new_dentry->inode = new_inode;
	new_dentry->hash = strhash(name);
	memset(new_dentry->name, 0, 255);
	memcpy(new_dentry->name, name, name_length);
	new_dentry->head = NULL;
	new_dentry->ref_count = 0;
	ATOMIC_INCREMENT(&(dentry->ref_count));
	new_dentry->prev = NULL;
	new_dentry->next = dentry->head;
	new_dentry->parent = dentry;
	new_dentry->next = dentry->head;
	if (dentry->head != NULL) {
		dentry->head->prev = new_dentry;
	}
	dentry->head = new_dentry;
	mutex_init(&(new_dentry->mutex));
	return new_dentry;
}

struct vfs_dentry *vfs_trace_mounts(struct vfs_dentry *dentry) {
	while (true) {
		struct vfs_superblock *sb = dentry->inode->mount;
		if (sb == NULL) {
			break;
		}
		struct vfs_dentry *next = sb->root;
		ATOMIC_INCREMENT(&(next->ref_count));
		mutex_unlock(&(dentry->mutex));
		mutex_lock(&(next->mutex));
		ATOMIC_DECREMENT(&(dentry->ref_count));
		dentry = next;
	}
	return dentry;
}

struct vfs_dentry *vfs_dentry_drop_get_parent(struct vfs_dentry *dentry) {
	if (dentry->parent != NULL) {
		return dentry->parent;
	} else {
		return dentry->inode->sb->mount_location;
	}
}

void vfs_dentry_drop_rec(struct vfs_dentry *dentry) {
	while (dentry != NULL) {
		struct vfs_dentry *parent = vfs_dentry_drop_get_parent(dentry);
		if (parent == NULL) {
			ATOMIC_DECREMENT(&(dentry->ref_count));
			mutex_unlock(&(dentry->mutex));
			return;
		}
		mutex_lock(&(parent->mutex));
		if (!((dentry->parent == NULL) ? vfs_drop_mount
									   : vfs_drop_dentry)(dentry)) {
			mutex_unlock(&(dentry->mutex));
			mutex_unlock(&(parent->mutex));
			return;
		}
		dentry = parent;
	}
}

struct vfs_dentry *vfs_dentry_walk_to_child(struct vfs_dentry *dentry,
											const char *name) {
	if (strlen(name) > VFS_MAX_NAME_LENGTH) {
		vfs_dentry_drop_rec(dentry);
		return NULL;
	}
	struct vfs_dentry *result = vfs_dentry_lookup(dentry, name);
	if (result == NULL) {
		result = vfs_dentry_load_child(dentry, name);
		if (result == NULL) {
			vfs_dentry_drop_rec(dentry);
			return NULL;
		}
	}
	ATOMIC_INCREMENT(&(result->ref_count));
	mutex_unlock(&(dentry->mutex));
	mutex_lock(&(result->mutex));
	ATOMIC_DECREMENT(&(dentry->ref_count));
	return vfs_trace_mounts(result);
}

struct vfs_dentry *vfs_dentry_walk(struct vfs_dentry *dentry,
								   struct path_splitter *splitter) {
	const char *name = path_splitter_get(splitter);
	struct vfs_dentry *current = dentry;
	while (name != NULL) {
		if (*name == '\0' || streq(name, ".")) {
			goto next;
		} else if (streq(name, "..")) {
			current = vfs_dentry_walk_to_parent(current);
		} else {
			current = vfs_dentry_walk_to_child(current, name);
		}
		if (current == NULL) {
			return NULL;
		}
	next:
		name = path_splitter_advance(splitter);
	}
	return current;
}

struct vfs_dentry *vfs_walk_from_root(const char *path) {
	struct path_splitter splitter;
	if (!path_splitter_init(path, &splitter)) {
		return NULL;
	}
	struct vfs_dentry *fs_root = vfs_root->root;
	if (fs_root == NULL) {
		kmsg_err("Virtual File System", "Filesystem root should never be NULL");
	}
	mutex_lock(&(fs_root->mutex));
	ATOMIC_INCREMENT(&(fs_root->ref_count));
	fs_root = vfs_trace_mounts(fs_root);
	if (fs_root == NULL) {
		kmsg_err("Virtual File System",
				 "vfs_trace_mounts should never return NULL");
	}
	struct vfs_dentry *result = vfs_dentry_walk(fs_root, &splitter);
	path_splitter_dispose(&splitter);
	return result;
}

bool vfs_mount_initialized(const char *path, struct vfs_superblock *sb) {
	struct vfs_dentry *dir = vfs_walk_from_root(path);
	if (dir == NULL) {
		vfs_dentry_drop_rec(dir);
		return false;
	}
	if (dir->inode->stat.st_type != VFS_DT_DIR) {
		vfs_dentry_drop_rec(dir);
		return false;
	}
	if (dir->inode->mount != NULL) {
		kmsg_err("Virtual File System",
				 "vfs_trace_mounts is not doing its job");
	}
	dir->inode->mount = sb;
	sb->mount_location = dir;
	ino_t root_id = 1;
	if (sb->type->get_root_inode != NULL) {
		root_id = sb->type->get_root_inode(sb->ctx);
	}
	if (root_id == 0) {
		vfs_dentry_drop_rec(dir);
		return false;
	}
	for (size_t i = 0; i < VFS_ICACHE_MODULO; ++i) {
		sb->inode_lists[i] = NULL;
	}
	mutex_init(&(sb->mutex));
	struct vfs_inode *inode = vfs_get_inode(sb, root_id);
	if (inode == NULL) {
		vfs_dentry_drop_rec(dir);
		return false;
	}
	struct vfs_dentry *dentry = ALLOC_OBJ(struct vfs_dentry);
	dentry->hash = 0;
	if (dentry == NULL) {
		vfs_dentry_drop_rec(dir);
		vfs_drop_inode(inode->sb, inode);
		return false;
	}
	dentry->parent = dentry->head = dentry->next = dentry->prev = NULL;
	dentry->ref_count = 1;
	dentry->inode = inode;
	mutex_init(&(dentry->mutex));
	sb->root = dentry;
	mutex_lock(&vfs_mutex);
	sb->next_mounted = vfs_sb_list;
	if (vfs_sb_list != NULL) {
		vfs_sb_list->prev_mounted = sb;
	}
	sb->prev_mounted = NULL;
	vfs_sb_list = sb;
	mutex_unlock(&vfs_mutex);
	mutex_unlock(&(dir->mutex));
	return true;
}

struct vfs_superblock_type *vfs_get_type(const char *fs_type) {
	mutex_lock(&vfs_mutex);
	struct vfs_superblock_type *current = vfs_sb_types;
	while (current != NULL) {
		if (streq(current->fs_name, fs_type)) {
			mutex_unlock(&vfs_mutex);
			if (current->mount == NULL) {
				return NULL;
			}
			return current;
		}
		current = current->next;
	}
	mutex_unlock(&vfs_mutex);
	return NULL;
}

bool vfs_mount_user(const char *path, const char *devpath,
					const char *fs_type_name) {
	struct vfs_superblock_type *fs_type = vfs_get_type(fs_type_name);
	if (fs_type == NULL) {
		return false;
	}
	if (fs_type->mount == NULL) {
		return false;
	}
	struct vfs_superblock *sb = fs_type->mount(devpath);
	if (sb == NULL) {
		return false;
	}
	if (!vfs_mount_initialized(path, sb)) {
		if (!(fs_type->umount != NULL)) {
			fs_type->umount(sb);
		}
		FREE_OBJ(sb);
		return false;
	}
	return true;
}

bool vfs_unmount(const char *path) {
	struct vfs_dentry *fs_root = vfs_walk_from_root(path);
	if (fs_root == NULL) {
		return false;
	}
	struct vfs_dentry *location = fs_root->inode->sb->mount_location;
	mutex_lock(&(location->mutex));
	struct vfs_superblock *mounted_sb = location->inode->mount;
	if (mounted_sb == NULL) {
		mutex_unlock(&(location->mutex));
		vfs_dentry_drop_rec(fs_root);
		return false;
	}
	location->inode->mount = NULL;
	ATOMIC_DECREMENT(&(fs_root->ref_count));
	mutex_unlock(&(location->mutex));
	vfs_dentry_drop_rec(fs_root);
	return true;
}

void vfs_init(struct vfs_superblock *sb) {
	for (size_t i = 0; i < VFS_ICACHE_MODULO; ++i) {
		sb->inode_lists[i] = NULL;
	}
	ino_t root_inode = 1;
	if (sb->type->get_root_inode != NULL) {
		root_inode = sb->type->get_root_inode(sb->ctx);
	}
	if (root_inode == 0) {
		kmsg_err("Virtual File System", "rootfs root inode number is zero");
	}
	struct vfs_inode *inode = vfs_get_inode(sb, root_inode);
	if (inode == NULL) {
		kmsg_err("Virtual File System", "Failed to load rootfs root inode");
	}
	struct vfs_dentry *dentry = ALLOC_OBJ(struct vfs_dentry);
	if (dentry == NULL) {
		kmsg_err("Virtual File System", "Failed to allocate root dirent");
	}
	dentry->parent = dentry->head = dentry->prev = dentry->next = NULL;
	dentry->hash = 0;
	dentry->inode = inode;
	dentry->ref_count = 1;
	sb->root = dentry;
	sb->mount_location = NULL;
	sb->next_mounted = sb->prev_mounted;
	vfs_sb_list = sb;
	vfs_root = sb;
	mutex_init(&vfs_mutex);
	mutex_init(&(dentry->mutex));
}

void vfs_register_filesystem(struct vfs_superblock_type *type) {
	mutex_lock(&vfs_mutex);
	struct vfs_superblock_type *current = vfs_sb_types;
	type->next = current;
	if (current != NULL) {
		current->prev = type;
	}
	type->prev = NULL;
	vfs_sb_types = type;
	mutex_unlock(&vfs_mutex);
}

struct fd *vfs_open(const char *path, int perm) {
	struct vfs_dentry *file = vfs_walk_from_root(path);
	if (file == NULL) {
		return false;
	}
	if (file->inode->ops->open == NULL) {
		vfs_dentry_drop_rec(file);
	}
	mutex_unlock(&(file->mutex));
	struct fd *fd = file->inode->ops->open(file->inode, perm);
	if (fd == NULL) {
		mutex_lock(&(file->mutex));
		vfs_dentry_drop_rec(file);
	}
	fd->dentry = file;
	return fd;
}

void vfs_finalize(struct fd *fd) {
	mutex_lock(&(fd->dentry->mutex));
	vfs_dentry_drop_rec(fd->dentry);
}
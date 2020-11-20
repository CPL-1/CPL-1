#ifndef __DEVFS_H_INCLUDED__
#define __DEVFS_H_INCLUDED__

#include <common/core/fd/vfs.h>
#include <common/misc/utils.h>

void devfs_init();
bool devfs_register_inode(const char *name, struct vfs_inode *inode);

#endif
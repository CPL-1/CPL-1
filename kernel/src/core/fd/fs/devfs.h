#ifndef __DEVFS_H_INCLUDED__
#define __DEVFS_H_INCLUDED__

#include <core/fd/vfs.h>
#include <utils/utils.h>

void devfs_init();
bool devfs_register_inode(const char *name, struct vfs_inode *inode);

#endif
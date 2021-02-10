#ifndef __DEVFS_H_INCLUDED__
#define __DEVFS_H_INCLUDED__

#include <common/core/fd/vfs.h>
#include <common/misc/utils.h>

void DevFS_Initialize();
bool DevFS_RegisterInode(const char *name, struct VFS_Inode *inode);

#endif

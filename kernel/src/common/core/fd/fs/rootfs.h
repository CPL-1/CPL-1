#ifndef __ROOTFS_H_INCLUDED__
#define __ROOTFS_H_INCLUDED__

#include <common/core/fd/vfs.h>
#include <common/misc/utils.h>

struct VFS_Superblock *RootFS_MakeSuperblock();

#endif
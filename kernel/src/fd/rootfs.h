#ifndef __ROOTFS_H_INCLUDED__
#define __ROOTFS_H_INCLUDED__

#include <fd/vfs.h>
#include <utils.h>

struct vfs_superblock *rootfs_make_superblock();

#endif
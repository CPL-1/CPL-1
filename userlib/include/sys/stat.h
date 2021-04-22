#ifndef __CPL1_LIBC_STAT_H_INCLUDED__
#define __CPL1_LIBC_STAT_H_INCLUDED__

#include <sys/types.h>

struct stat {
	mode_t st_type;
	off_t st_size;
	blksize_t st_blksize;
	blkcnt_t st_blkcnt;
};

int fstat(int fd, struct stat *buf);

#endif

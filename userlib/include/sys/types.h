#ifndef __SYS_TYPES_H_INCLUDED__
#define __SYS_TYPES_H_INCLUDED__

#include <stddef.h>
#include <stdint.h>

typedef int64_t time_t;
typedef uint64_t suseconds_t;

typedef uint32_t ino_t;
typedef int64_t off_t;
typedef int64_t blksize_t;
typedef int64_t blkcnt_t;
typedef int mode_t;

struct stat {
	mode_t stType;
	off_t stSize;
	blksize_t stBlksize;
	blkcnt_t stBlkcnt;
};

#endif

#ifndef __CPL1_LIBC_DIRENT_H_INCLUDED__
#define __CPL1_LIBC_DIRENT_H_INCLUDED__

#include <sys/types.h>

#define DT_UNKNOWN 0
#define DT_FIFO 1
#define DT_CHR 2
#define DT_DIR 4
#define DT_BLK 6
#define DT_REG 8
#define DT_LNK 9
#define DT_SOCK 12
#define DT_WHT 14

struct dirent {
	ino_t d_ino;
	char d_name[256];
};

int getdents(int fd, struct dirent *entries, int count);

#endif

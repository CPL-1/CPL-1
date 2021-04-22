#ifndef __FCNTL_H_INCLUDED__
#define __FCNTL_H_INCLUDED__

#define O_RDONLY 0
#define O_WRONLY 1
#define O_RDWR 2

int open(const char *path, int perm);

#endif

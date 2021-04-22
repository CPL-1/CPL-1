#ifndef __CPL1_LIBC_MMAN_H_INCLUDED__
#define __CPL1_LIBC_MMAN_H_INCLUDED__

#include <stddef.h>

#define PROT_NONE 0x00
#define PROT_READ 0x01
#define PROT_WRITE 0x02
#define PROT_EXEC 0x04
#define PROT_MASK (PROT_READ | PROT_WRITE | PROT_EXEC)

#define MAP_SHARED 0x01
#define MAP_PRIVATE 0x02
#define MAP_FIXED 0x10
#define MAP_ANON 0x1000
#define MAP_FILE 0x0000
#define MAP_FAIL ((void *)-1)

void *mmap(void *addr, size_t length, int prot, int flags, int fd, long offset);
int munmap(void *addr, size_t length);

#endif

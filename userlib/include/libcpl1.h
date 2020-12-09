#ifndef __CPL1_SYSCALLS_H_INCLUDED__
#define __CPL1_SYSCALLS_H_INCLUDED__

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

#define O_RDONLY 0
#define O_WRONLY 1
#define O_RDWR 2

int CPL1_SyscallOpen(const char *path, int perm);
int CPL1_SyscallRead(int fd, char *buf, int size);
int CPL1_SyscallWrite(int fd, const char *buf, int size);
int CPL1_SyscallClose(int fd);
void CPL1_SyscallExit(int exitCode);
void *CPL1_SyscallMemoryMap(void *addr, size_t length, int prot, int flags, int fd, long offset);
int CPL1_SyscallMemoryUnmap(void *addr, size_t length);

#endif
#ifndef __CPL1_SYSCALLS_H_INCLUDED__
#define __CPL1_SYSCALLS_H_INCLUDED__

int CPL1_SyscallOpen(const char *path, int perm);
int CPL1_SyscallRead(int fd, char *buf, int size);
int CPL1_SyscallWrite(int fd, const char *buf, int size);
int CPL1_SyscallClose(int fd);
void CPL1_SyscallExit(int exitCode);

#endif
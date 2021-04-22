#ifndef __CPL1_LIBC_UNISTD_H_INCLUDED__
#define __CPL1_LIBC_UNISTD_H_INCLUDED__

#define STDIN_FILENO 0
#define STDOUT_FILENO 1
#define STDERR_FILENO 2

int read(int fd, char *buf, int size);
int write(int fd, const char *buf, int size);
int fork();
int execve(const char *fname, char const *argp[], char const *envp[]);
int close(int fd);
int getcwd(char *buf, int length);
int chdir(const char *path);
int fchdir(int fd);
int getpid();
int getppid();

#endif

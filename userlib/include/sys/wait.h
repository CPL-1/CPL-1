#ifndef __CPL1_LIBC_WAIT_H_INCLUDED__
#define __CPL1_LIBC_WAIT_H_INCLUDED__

#define WNOHANG 1
#define WUNTRACED 2

struct rusage;
int wait4(int pid, int *wstatus, int options, struct rusage *rusage);

#endif

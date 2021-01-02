#ifndef __ABIS_H_INCLUDED__
#define __ABIS_H_INCLUDED__

#define PROT_NONE 0x00
#define PROT_READ 0x01
#define PROT_WRITE 0x02
#define PROT_EXEC 0x04
#define PROT_MASK (PROT_READ | PROT_WRITE | PROT_EXEC)

#define MAP_SHARED 0x01
#define MAP_PRIVATE 0x02
#define MAP_ANON 0x1000
#define MAP_FIXED 0x1

#define WNOHANG 1
#define WUNTRACED 2

#define SIGHUP 1
#define SIGINT 2
#define SIGQUIT 3
#define SIGILL 4
#define SIGTRAP 5
#define SIGABRT 6
#define SIGFPE 8
#define SIGKILL 9
#define SIGBUS 10
#define SIGSEGV 11
#define SIGSYS 12
#define SIGPIPE 13
#define SIGALRM 14
#define SIGTERM 15
#define SIGURG 16
#define SIGSTOP 17
#define SIGTSTP 18
#define SIGCONT 19
#define SIGCHLD 20
#define SIGTTIN 21
#define SIGTTOU 22
#define SIGIO 23
#define SIGXCPU 24
#define SIGXFSZ 25
#define SIGVTALRM 26
#define SIGINFO 29
#define SIGUSR1 30
#define SIGUSR2 31
#define SIGTHR 32


#endif
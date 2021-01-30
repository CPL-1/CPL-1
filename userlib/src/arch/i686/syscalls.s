bits 32

%macro make_syscall 2
global %1
%1:
    mov eax, %2
    int 0x80
    ret
%endmacro

make_syscall exit, 1
make_syscall fork, 2
make_syscall read, 3
make_syscall write, 4
make_syscall open, 5
make_syscall close, 6
make_syscall wait4, 11
make_syscall chdir, 12
make_syscall fchdir, 13
make_syscall getpid, 20
make_syscall getppid, 39
make_syscall execve, 59
make_syscall munmap, 73
make_syscall getdents, 99
make_syscall mmap, 197
make_syscall getcwd, 304
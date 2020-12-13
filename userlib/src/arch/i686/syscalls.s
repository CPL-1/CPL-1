bits 32

global open
global read
global write
global close
global exit
global mmap
global munmap
global fork
global execve
global wait4

open:
    times 2 push dword [esp + 8]
    sub esp, 8
    mov eax, 5
    int 0x80
    add esp, 16
    ret

read:
    times 3 push dword [esp + 12]
    sub esp, 4
    mov eax, 3
    int 0x80
    add esp, 16
    ret

write:
    times 3 push dword [esp + 12]
    sub esp, 4
    mov eax, 4
    int 0x80
    add esp, 16
    ret
    ret

close:
    push dword [esp + 4]
    sub esp, 12
    mov eax, 6
    int 0x80
    add esp, 16
    ret

exit:
    push dword [esp + 4]
    sub esp, 12
    mov eax, 1
    int 0x80
    add esp, 16
    ret

mmap:
    times 6 push dword [esp + 24]
    sub esp, 8
    mov eax, 197
    int 0x80
    add esp, 32
    ret

munmap:
    times 2 push dword [esp + 8]
    sub esp, 8
    mov eax, 73
    int 0x80
    add esp, 16
    ret

fork:
    mov eax, 2
    int 0x80
    ret

execve:
    times 3 push dword [esp + 12]
    sub esp, 4
    mov eax, 59
    int 0x80
    add esp, 16
    ret

wait4:
    times 4 push dword [esp + 16]
    mov eax, 400
    int 0x80
    add esp, 16
    ret
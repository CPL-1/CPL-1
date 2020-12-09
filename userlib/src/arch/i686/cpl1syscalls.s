bits 32

global CPL1_SyscallOpen
global CPL1_SyscallRead
global CPL1_SyscallWrite
global CPL1_SyscallClose
global CPL1_SyscallExit
global CPL1_SyscallMemoryMap
global CPL1_SyscallMemoryUnmap

CPL1_SyscallOpen:
    times 2 push dword [esp + 8]
    sub esp, 8
    mov eax, 5
    int 0x80
    add esp, 16
    ret

CPL1_SyscallRead:
    times 3 push dword [esp + 12]
    sub esp, 4
    mov eax, 3
    int 0x80
    add esp, 16
    ret

CPL1_SyscallWrite:
    times 3 push dword [esp + 12]
    sub esp, 4
    mov eax, 4
    int 0x80
    add esp, 16
    ret
    ret

CPL1_SyscallClose:
    push dword [esp + 4]
    sub esp, 12
    mov eax, 6
    int 0x80
    add esp, 16
    ret

CPL1_SyscallExit:
    push dword [esp + 4]
    sub esp, 12
    mov eax, 1
    int 0x80
    add esp, 16
    ret

CPL1_SyscallMemoryMap:
    times 6 push dword [esp + 24]
    sub esp, 8
    mov eax, 197
    int 0x80
    add esp, 32
    ret

CPL1_SyscallMemoryUnmap:
    times 2 push dword [esp + 8]
    sub esp, 8
    mov eax, 73
    int 0x80
    add esp, 16
    ret
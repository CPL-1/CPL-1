bits 32

global CPL1_SyscallOpen
global CPL1_SyscallRead
global CPL1_SyscallWrite
global CPL1_SyscallClose
global CPL1_SyscallExit

CPL1_SyscallOpen:
    push dword [esp + 8]
    push dword [esp + 8]
    sub esp, 8
    mov eax, 5
    int 0x80
    add esp, 16
    ret

CPL1_SyscallRead:
    push dword [esp + 12]
    push dword [esp + 12]
    push dword [esp + 12]
    sub esp, 4
    mov eax, 3
    int 0x80
    add esp, 16
    ret

CPL1_SyscallWrite:
    push dword [esp + 12]
    push dword [esp + 12]
    push dword [esp + 12]
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
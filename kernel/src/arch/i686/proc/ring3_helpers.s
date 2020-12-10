bits 32

global i686_Ring3_SyscallEntry
global i686_Ring3_Switch

extern i686_Ring3_SyscallTable
extern i686_Ring3_SyscallTableSize

section .text
i686_Ring3_Switch:
    mov ax, 0x33
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    mov eax, [esp + 4]
    mov ebx, [esp + 8]
    mov ecx, 1 << 12 | 1 << 9
    push 0x33
    push ebx
    push ecx
    push 0x2b
    push eax

    xor eax, eax
    mov ebx, eax
    mov edx, eax
    mov ecx, eax
    mov edi, eax
    mov esi, eax
    mov ebp, eax

    iretd

i686_Ring3_SyscallEntry:
    pusha
    mov ecx, dword [i686_Ring3_SyscallTableSize]
    cmp eax, ecx
    jge .fail
    mov ebx, [i686_Ring3_SyscallTable + 4 * eax]
    cmp ebx, 0
    je .fail
    
    push es
    push fs
    push gs
    push ds

    mov eax, 0x21
    mov es, eax
    mov ds, eax
    mov fs, eax
    mov gs, eax

    push esp
    call ebx
    add esp, 4

    pop gs
    pop fs
    pop ds
    pop es

    popa
    iretd

.fail:
    popa
    mov eax, -1
    iretd
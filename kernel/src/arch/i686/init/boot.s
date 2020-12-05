bits 32

global i686_Stivale_EntryPoint
extern i686_KernelInit_Main

KERNEL_MAPPING_BASE: equ 0xc0000000

section .bss
align 4096
m_bootStackBottom:
resb 65536
m_bootStackTop:
m_bootStackTopPhys: equ m_bootStackTop - KERNEL_MAPPING_BASE
m_bootPageTableDirectory:
resb 4096
m_bootPageTableDirectoryPhys: equ m_bootPageTableDirectory - KERNEL_MAPPING_BASE
m_bootPageTable:
resb 4096
m_bootPageTablePhys: equ m_bootPageTable - KERNEL_MAPPING_BASE

section .stivalehdr
dd m_bootStackTopPhys
dd 0
dw 1
dw 0
dw 0
dw 0
dq 0

section .inittext
i686_Stivale_EntryPoint:
    pop ebx
    pop ebx
    mov edi, m_bootPageTablePhys
    xor esi, esi
    mov ecx, 1024
.mapping_loop:
    cmp ecx, 0
    je .mapping_done
    mov edx, esi
    or edx, 0b11
    mov dword [edi], edx
    add esi, 0x1000
    add edi, 4
    dec ecx
    jmp .mapping_loop
.mapping_done:
    mov ebp, m_bootPageTablePhys
    or ebp, 0b11
    mov dword [m_bootPageTableDirectoryPhys], ebp
    mov dword [m_bootPageTableDirectoryPhys + 768 * 4], ebp
    mov ecx, m_bootPageTableDirectoryPhys
    mov cr3, ecx
    mov ecx, cr0
    or ecx, 0x80010000
    mov cr0, ecx
    jmp i686_higher_half_start

section .text
i686_higher_half_start:
    mov esp, m_bootStackTop
    push ebx
    call i686_KernelInit_Main
    pop ebx
    cli
.halted:
    hlt
    jmp .halted

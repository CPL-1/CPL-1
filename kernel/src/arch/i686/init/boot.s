bits 32

global i686_Stivale_EntryPoint
extern i686_KernelInit_DoInitialization

KERNEL_MAPPING_BASE: equ 0xc0000000

section .bss
align 4096
i686_BootStackBottom:
resb 65536
i686_BootStackTop:
i686_BootStackTopPhys: equ i686_BootStackTop - KERNEL_MAPPING_BASE
i686_BootPageTableDirectory:
resb 4096
i686_BootPageTableDirectoryPhys: equ i686_BootPageTableDirectory - KERNEL_MAPPING_BASE
i686_BootPageTable:
resb 4096
i686_BootPageTablePhys: equ i686_BootPageTable - KERNEL_MAPPING_BASE

section .stivalehdr
dd i686_BootStackTopPhys
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
    mov edi, i686_BootPageTablePhys
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
    mov ebp, i686_BootPageTablePhys
    or ebp, 0b11
    mov dword [i686_BootPageTableDirectoryPhys], ebp
    mov dword [i686_BootPageTableDirectoryPhys + 768 * 4], ebp
    mov ecx, i686_BootPageTableDirectoryPhys
    mov cr3, ecx
    mov ecx, cr0
    or ecx, 0x80010000
    mov cr0, ecx
    jmp i686_higher_half_start

section .text
i686_higher_half_start:
    mov esp, i686_BootStackTop
    push ebx
    call i686_KernelInit_DoInitialization
    pop ebx
    cli
.halted:
    hlt
    jmp .halted

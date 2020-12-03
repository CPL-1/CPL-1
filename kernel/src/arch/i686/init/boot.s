bits 32

global i686_stivale_start
extern i686_kernel_main

KERNEL_MAPPING_BASE: equ 0xc0000000

section .bss
align 4096
i686_stack_bottom:
resb 65536
i686_stack_top:
i686_stack_top_phys: equ i686_stack_top - KERNEL_MAPPING_BASE
i686_boot_page_dir:
resb 4096
i686_boot_page_dir_phys: equ i686_boot_page_dir - KERNEL_MAPPING_BASE
i686_boot_page_table:
resb 4096
i686_boot_page_table_phys: equ i686_boot_page_table - KERNEL_MAPPING_BASE

section .stivalehdr
dd i686_stack_top_phys
dd 0
dw 0
dw 0
dw 0
dw 0
dq 0

section .inittext
i686_stivale_start:
    pop ebx
    pop ebx
    mov edi, i686_boot_page_table_phys
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
    mov ebp, i686_boot_page_table_phys
    or ebp, 0b11
    mov dword [i686_boot_page_dir_phys], ebp
    mov dword [i686_boot_page_dir_phys + 768 * 4], ebp
    mov ecx, i686_boot_page_dir_phys
    mov cr3, ecx
    mov ecx, cr0
    or ecx, 0x80010000
    mov cr0, ecx
    jmp i686_higher_half_start

section .text
i686_higher_half_start:
    mov esp, i686_stack_top
    push ebx
    call i686_kernel_main
    pop ebx
    cli
.halted:
    hlt
    jmp .halted

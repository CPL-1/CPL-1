bits 32

global i386_mb1_start
extern i386_kernel_main

HEADER_ALIGN: equ 1 << 0
HEADER_MEMINFO: equ 1 << 1
HEADER_FLAGS: equ HEADER_ALIGN | HEADER_MEMINFO
HEADER_MAGIC: equ 0x1BADB002
HEADER_CHECKSUM: equ -(HEADER_MAGIC + HEADER_FLAGS)

KERNEL_MAPPING_BASE: equ 0xC0000000

section .multiboot
align 4
dd HEADER_MAGIC
dd HEADER_FLAGS
dd HEADER_CHECKSUM

section .bss
align 4096
stack_bottom:
resb 65536
stack_top:
stack_top_phys: equ stack_top - KERNEL_MAPPING_BASE
boot_page_dir:
resb 4096
boot_page_dir_phys: equ boot_page_dir - KERNEL_MAPPING_BASE
boot_page_table:
resb 4096
boot_page_table_phys: equ boot_page_table - KERNEL_MAPPING_BASE

section .inittext
i386_mb1_start:
    mov esp, stack_top_phys
    mov edi, boot_page_table_phys
    xor esi, esi
    mov ecx, 1024
mapping_loop:
    cmp ecx, 0
    je mapping_done
    mov edx, esi
    or edx, 0b11
    mov dword [edi], edx
    add esi, 0x1000
    add edi, 4
    dec ecx
    jmp mapping_loop
mapping_done:
    mov ebp, boot_page_table_phys
    or ebp, 0b11
    mov dword [boot_page_dir_phys], ebp
    mov dword [boot_page_dir_phys + 768 * 4], ebp
    mov ecx, boot_page_dir_phys
    mov cr3, ecx
    mov ecx, cr0
    or ecx, 0x80010000
    mov cr0, ecx
    jmp higher_halfi386_mb1_start

section .text
higher_halfi386_mb1_start:
    mov esp, stack_top
    push ebx
    call i386_kernel_main
    pop ebx
    cli
.halted:
    hlt
    jmp .halted

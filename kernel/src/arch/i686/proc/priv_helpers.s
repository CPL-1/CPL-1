bits 32

global i686_priv_call_ring0
global i686_priv_call_ring0_isr

section .text

i686_priv_call_ring0:
    pop edx
    pop ecx
    int 0xff
    push ecx
    push edx
    ret

i686_priv_call_ring0_isr:
    pusha

    push es
    push fs
    push gs
    push ds

    mov eax, 0x10
    mov es, eax
    mov fs, eax
    mov gs, eax
    mov ds, eax

    push edx
    call ecx
    pop edx

    pop ds
    pop gs
    pop fs
    pop es
    popa

    iretd
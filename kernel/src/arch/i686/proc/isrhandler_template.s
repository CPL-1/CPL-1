bits 32

global i686_ISR_TemplateBegin
global i686_ISR_TemplateEnd

i686_ISR_TemplateBegin:
    pusha

    push es
    push fs
    push gs
    push ds

    mov eax, 0x10
    mov es, eax
    mov ds, eax
    mov fs, eax
    mov gs, eax

    push esp
    push strict dword 0x11111111

    mov strict eax, 0x22222222
    call eax
    add esp, 8

    ; Make sure that interrupts are enabled on exit
    ; with modifying EFLAGS register in interrupt frame
    mov eax, dword [esp + 56]
    or eax, 1 << 9
    mov dword [esp + 56], eax

    pop gs
    pop fs
    pop ds
    pop es

    popa
    iretd
i686_ISR_TemplateEnd:
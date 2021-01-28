bits 32

global i686_ISR_TemplateBegin
global i686_ISR_TemplateEnd
global i686_ISR_TemplateErrorCodeBegin
global i686_ISR_TemplateErrorCodeEnd

i686_ISR_TemplateBegin:
    push dword 0
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

    pop gs
    pop fs
    pop ds
    pop es

    popa
    add esp, 4
    iretd
i686_ISR_TemplateEnd:

i686_ISR_TemplateErrorCodeBegin:
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

    pop gs
    pop fs
    pop ds
    pop es

    popa
    add esp, 4
    iretd
i686_ISR_TemplateErrorCodeEnd:
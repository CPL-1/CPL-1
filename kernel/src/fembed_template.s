bits 32

global fembed_template
global fembed_template_end

fembed_template:
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
    pop ds
    pop gs
    pop fs
    pop es

    popa
    iretd
fembed_template_end:
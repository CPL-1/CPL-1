bits 32

global i686_isr_with_ctx_template_begin
global i686_isr_with_ctx_template_end

i686_isr_with_ctx_template_begin:
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
i686_isr_with_ctx_template_end:
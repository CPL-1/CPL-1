bits 32

global pit_interrupt_handler
extern pit_call_handler

pit_interrupt_handler:
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

    call pit_call_handler

    add esp, 4
    pop ds
    pop gs
    pop fs
    pop es

    popa
    iretd

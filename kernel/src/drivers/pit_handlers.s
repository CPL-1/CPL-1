bits 32

global pit_interrupt_handler
extern pit_call_handler

pit_interrupt_handler:
    pusha

    xor eax, eax

    mov ax, es
    push eax
    mov ax, fs
    push eax
    mov ax, gs
    push eax
    mov ax, ds
    push eax

    mov ax, 0x10
    mov es, ax
    mov ds, ax
    mov fs, ax
    mov gs, ax

    mov eax, esp
    push eax

    call pit_call_handler

    add esp, 4
    pop eax
    mov ds, ax
    pop eax
    mov gs, ax
    pop eax
    mov fs, ax
    pop eax
    mov es, ax

    popa
    iretd


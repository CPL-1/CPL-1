bits 32

global ring1_switch

ring1_switch:
    push eax
    ; Enable IO operations for ring 1
    pushfd
    pop eax
    or eax, 1 << 12
    push eax
    popfd

    mov ax, 0x21
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    mov eax, esp
    push 0x21
    push eax
    pushf
    push 0x19
    push end
    iret
end:
    pop eax
    ret
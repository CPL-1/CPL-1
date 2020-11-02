bits 32

global ring1_switch

ring1_switch:
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
    ret
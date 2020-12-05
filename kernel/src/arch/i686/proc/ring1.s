bits 32

global Ring1_Switch
Ring1_Switch:
    push eax
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

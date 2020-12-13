bits 32

global memcpy
global memset

memcpy:
    push ecx
    push edi
    push esi
    mov edi, dword [esp + 16]
    mov esi, dword [esp + 20]
    mov ecx, dword [esp + 24]
    rep movsb
    pop esi
    pop edi
    pop ecx
    ret

memset:
    push ecx
    push eax
    push edi
    mov edi, dword [esp + 16]
    mov eax, dword [esp + 20]
    mov ecx, dword [esp + 24]
    rep stosb
    pop edi
    pop eax
    pop ecx
    ret
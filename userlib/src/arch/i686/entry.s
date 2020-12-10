bits 32

global _start
extern main

section .text
_start:
	call main

	push eax
	mov eax, 1
	sub esp, 12
	int 0x80
	

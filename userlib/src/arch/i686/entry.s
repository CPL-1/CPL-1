bits 32

global _start
extern main

section .bss
stackBottom:
	resb 65536
stackTop:

section .text
_start:
	mov esp, stackTop
	call main

	push eax
	mov eax, 1
	sub esp, 12
	int 0x80
	

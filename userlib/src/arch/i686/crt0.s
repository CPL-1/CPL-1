bits 32

global _start
extern main
extern Libc_Init

section .text
_start:
	push dword main
	call Libc_Init

	push eax
	mov eax, 1
	sub esp, 12
	int 0x80

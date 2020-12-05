#ifndef __I686_PORTS_H_INCLUDED__
#define __I686_PORTS_H_INCLUDED__

#include <common/misc/utils.h>

static INLINE void i686_Ports_WriteByte(UINT16 port, UINT8 val) {
	asm volatile("outb %0, %1" : : "a"(val), "Nd"(port));
}

static INLINE UINT8 i686_Ports_ReadByte(UINT16 port) {
	UINT8 ret;
	asm volatile("inb %1, %0" : "=a"(ret) : "Nd"(port));
	return ret;
}

static INLINE void i686_Ports_WriteWord(UINT16 port, UINT16 val) {
	asm volatile("outw %0, %1" : : "a"(val), "Nd"(port));
}

static INLINE UINT16 i686_Ports_ReadWord(UINT16 port) {
	UINT16 ret;
	asm volatile("inw %1, %0" : "=a"(ret) : "Nd"(port));
	return ret;
}

static INLINE void i686_Ports_WriteDoubleWord(UINT16 port, UINT32 val) {
	asm volatile("outl %0, %1" : : "a"(val), "Nd"(port));
}

static INLINE UINT32 i686_Ports_ReadDoubleWord(UINT16 port) {
	UINT32 ret;
	asm volatile("inl %1, %0" : "=a"(ret) : "Nd"(port));
	return ret;
};

#endif

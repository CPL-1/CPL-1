#ifndef __I686_PORTS_H_INCLUDED__
#define __I686_PORTS_H_INCLUDED__

#include <common/misc/utils.h>

static INLINE void i686_Ports_WriteByte(uint16_t port, uint8_t val) {
	asm volatile("outb %0, %1" : : "a"(val), "Nd"(port));
}

static INLINE uint8_t i686_Ports_ReadByte(uint16_t port) {
	uint8_t ret;
	asm volatile("inb %1, %0" : "=a"(ret) : "Nd"(port));
	return ret;
}

static INLINE void i686_Ports_WriteWord(uint16_t port, uint16_t val) {
	asm volatile("outw %0, %1" : : "a"(val), "Nd"(port));
}

static INLINE uint16_t i686_Ports_ReadWord(uint16_t port) {
	uint16_t ret;
	asm volatile("inw %1, %0" : "=a"(ret) : "Nd"(port));
	return ret;
}

static INLINE void i686_Ports_WriteDoubleWord(uint16_t port, uint32_t val) {
	asm volatile("outl %0, %1" : : "a"(val), "Nd"(port));
}

static INLINE uint32_t i686_Ports_ReadDoubleWord(uint16_t port) {
	uint32_t ret;
	asm volatile("inl %1, %0" : "=a"(ret) : "Nd"(port));
	return ret;
};

#endif

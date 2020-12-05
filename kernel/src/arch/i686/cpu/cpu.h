#ifndef __I686_CPU_H_INCLUDED__
#define __I686_CPU_H_INCLUDED__

#include <common/misc/utils.h>

static INLINE uint32_t i686_CPU_GetCR3() {
	uint32_t val;
	asm volatile("mov %%cr3, %0" : "=r"(val));
	return val;
}

static INLINE uint32_t i686_CPU_SetCR3(uint32_t val) {
	asm volatile("mov %0, %%cr3" : : "r"(val));
	return val;
}

static INLINE void i686_CPU_InvalidatePage(uint32_t vaddr) {
	asm volatile("invlpg (%0)" : : "b"(vaddr) : "memory");
}

static INLINE void i686_CPU_WaitForIOCompletition() {
	asm volatile("outb %%al, $0x80" : : "a"(0));
}

#endif

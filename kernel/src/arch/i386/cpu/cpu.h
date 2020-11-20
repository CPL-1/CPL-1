#ifndef __I386_CPU_H_INCLUDED__
#define __I386_CPU_H_INCLUDED__

#include <common/misc/utils.h>

static inline uint32_t i386_cpu_get_cr3() {
	uint32_t val;
	asm volatile("mov %%cr3, %0" : "=r"(val));
	return val;
}

static inline uint32_t i386_cpu_set_cr3(uint32_t val) {
	asm volatile("mov %0, %%cr3" : : "r"(val));
	return val;
}

static inline void i386_cpu_invlpg(uint32_t vaddr) {
	asm volatile("invlpg (%0)" : : "b"(vaddr) : "memory");
}

static inline void i386_cpu_io_wait() {
	asm volatile("outb %%al, $0x80" : : "a"(0));
}

#endif

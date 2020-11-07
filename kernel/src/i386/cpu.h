#ifndef __CPU_H_INCLUDED__
#define __CPU_H_INCLUDED__

#include <utils.h>

static inline uint32_t cpu_get_cr3() {
	uint32_t val;
	asm volatile("mov %%cr3, %0" : "=r"(val));
	return val;
}

static inline uint32_t cpu_set_cr3(uint32_t val) {
	asm volatile("mov %0, %%cr3" : : "r"(val));
	return val;
}

static inline void cpu_invlpg(uint32_t vaddr) {
	asm volatile("invlpg (%0)" : : "b"(vaddr) : "memory");
}

static inline void cpu_io_wait() {
	asm volatile("outb %%al, $0x80" : : "a"(0));
}

#endif

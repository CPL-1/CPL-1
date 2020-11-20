#include <arch/i386/cpu/cpu.h>
#include <arch/i386/cpu/cr3.h>

static uint32_t cr3;

void i386_cr3_init() { cr3 = i386_cpu_get_cr3(); }

void i386_cr3_set(uint32_t val) {
	i386_cpu_set_cr3(val);
	cr3 = val;
}

uint32_t i386_cr3_get() { return cr3; }
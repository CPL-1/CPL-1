#include <arch/i686/cpu/cpu.h>
#include <arch/i686/cpu/cr3.h>

static uint32_t cr3;

void i686_cr3_init() { cr3 = i686_cpu_get_cr3(); }

void i686_cr3_set(uint32_t val) {
	i686_cpu_set_cr3(val);
	cr3 = val;
}

uint32_t i686_cr3_get() { return cr3; }
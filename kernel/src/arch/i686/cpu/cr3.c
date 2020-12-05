#include <arch/i686/cpu/cpu.h>
#include <arch/i686/cpu/cr3.h>

static uint32_t i686_CR3;

void i686_CR3_Initialize() {
	i686_CR3 = i686_CPU_GetCR3();
}

void i686_CR3_Set(uint32_t val) {
	i686_CPU_SetCR3(val);
	i686_CR3 = val;
}

uint32_t i686_CR3_Get() {
	return i686_CR3;
}
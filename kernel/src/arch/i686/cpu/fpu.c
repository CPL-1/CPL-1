#include <arch/i686/cpu/cpu.h>
#include <arch/i686/cpu/fpu.h>
#include <common/misc/utils.h>
#include <hal/proc/extended.h>

size_t HAL_ExtendedStateSize = 512;

void i686_FPU_LoadControlWorld(uint16_t control) {
	asm volatile("fldcw %0;" ::"m"(control));
}

void i686_FPU_Enable() {
	// we assume running on i686 at least
	// so we don't bother checking
	uint32_t cr0 = i686_CPU_GetCR0();
	cr0 |= (1 << 1);  // normally set to inverse of bit 2, so shrug
	cr0 &= ~(1 << 2); // do not emulate FPU
	cr0 |= (1 << 5);  // use native exception for error handling
	i686_CPU_SetCR0(cr0);
	// update cr4
	uint32_t cr4 = i686_CPU_GetCR4();
	cr4 |= (1 << 9); // support fxsave and fxrstor instructions
	i686_CPU_SetCR4(cr4);
	// init FPU
	asm VOLATILE("fninit");
}

void HAL_ExtendedState_LoadFrom(char *buf) {
	asm VOLATILE("fxrstor %0" ::"m"(*(uint8_t *)buf));
}

void HAL_ExtendedState_StoreTo(char *buf) {
	asm VOLATILE("fxsave %0" ::"m"(*(uint8_t *)buf));
}
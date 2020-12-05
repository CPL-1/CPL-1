#include <arch/i686/cpu/gdt.h>
#include <arch/i686/cpu/tss.h>

struct i686_TSS_Layout {
	uint32_t : 32;
	uint32_t esp0;
	uint16_t ss0;
	uint16_t : 16;
	uint32_t esp1;
	uint16_t ss1;
	uint16_t : 16;
	uint32_t pad[20];
	uint16_t : 16;
	uint16_t iomapOffset;
} PACKED;

static struct i686_TSS_Layout i686_TSS;

void i686_TSS_Initialize() {
	memset(&i686_TSS, 0, sizeof(i686_TSS));
	i686_TSS.iomapOffset = sizeof(i686_TSS);
	ASM volatile("ltr %%ax" : : "a"(i686_GDT_GetTSSSegment()));
}

uint32_t i686_TSS_GetBase() {
	return (uint32_t)&i686_TSS;
}
uint32_t i686_TSS_GetLimit() {
	return sizeof(i686_TSS);
}

void i686_TSS_SetISRStack(uint32_t esp, uint16_t ss) {
	i686_TSS.esp0 = esp;
	i686_TSS.ss0 = ss;
}

void i686_TSS_SetKernelStack(uint32_t esp, uint16_t ss) {
	i686_TSS.esp1 = esp;
	i686_TSS.ss1 = ss;
}

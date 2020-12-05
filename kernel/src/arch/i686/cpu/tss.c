#include <arch/i686/cpu/gdt.h>
#include <arch/i686/cpu/tss.h>

struct i686_TSS_Layout {
	UINT32 : 32;
	UINT32 esp0;
	UINT16 ss0;
	UINT16 : 16;
	UINT32 esp1;
	UINT16 ss1;
	UINT16 : 16;
	UINT32 pad[20];
	UINT16 : 16;
	UINT16 iomapOffset;
} PACKED;

static struct i686_TSS_Layout i686_TSS;

void i686_TSS_Initialize() {
	memset(&i686_TSS, 0, sizeof(i686_TSS));
	i686_TSS.iomapOffset = sizeof(i686_TSS);
	ASM volatile("ltr %%ax" : : "a"(i686_GDT_GetTSSSegment()));
}

UINT32 i686_TSS_GetBase() {
	return (UINT32)&i686_TSS;
}
UINT32 i686_TSS_GetLimit() {
	return sizeof(i686_TSS);
}

void i686_TSS_SetISRStack(UINT32 esp, UINT16 ss) {
	i686_TSS.esp0 = esp;
	i686_TSS.ss0 = ss;
}

void i686_TSS_SetKernelStack(UINT32 esp, UINT16 ss) {
	i686_TSS.esp1 = esp;
	i686_TSS.ss1 = ss;
}

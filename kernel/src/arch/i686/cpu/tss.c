#include <arch/i686/cpu/gdt.h>
#include <arch/i686/cpu/tss.h>

struct m_TSS_Layout {
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

static struct m_TSS_Layout m_TSS;

void i686_TSS_Initialize() {
	memset(&m_TSS, 0, sizeof(m_TSS));
	m_TSS.iomapOffset = sizeof(m_TSS);
	ASM VOLATILE("ltr %%ax" : : "a"(i686_GDT_GetTSSSegment()));
}

uint32_t i686_TSS_GetBase() {
	return (uint32_t)&m_TSS;
}
uint32_t i686_TSS_GetLimit() {
	return sizeof(m_TSS);
}

void i686_TSS_SetISRStack(uint32_t esp, uint16_t ss) {
	m_TSS.esp0 = esp;
	m_TSS.ss0 = ss;
}

void i686_TSS_SetKernelStack(uint32_t esp, uint16_t ss) {
	m_TSS.esp1 = esp;
	m_TSS.ss1 = ss;
}

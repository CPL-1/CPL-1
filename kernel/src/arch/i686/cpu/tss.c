#include <arch/i686/cpu/gdt.h>
#include <arch/i686/cpu/tss.h>

struct i686_tss_layout {
	uint32_t : 32;
	uint32_t esp0;
	uint16_t ss0;
	uint16_t : 16;
	uint32_t esp1;
	uint16_t ss1;
	uint16_t : 16;
	uint32_t pad[20];
	uint16_t : 16;
	uint16_t iomap_offset;
} packed;

static struct i686_tss_layout tss;

void i686_tss_init() {
	memset(&tss, 0, sizeof(tss));
	tss.iomap_offset = sizeof(tss);
	asm volatile("ltr %%ax" : : "a"(i686_gdt_get_tss_segment()));
}

uint32_t i686_tss_get_base() { return (uint32_t)&tss; }
uint32_t i686_tss_get_limit() { return sizeof(tss); }

void i686_tss_set_dpl0_stack(uint32_t esp, uint16_t ss) {
	tss.esp0 = esp;
	tss.ss0 = ss;
}

void i686_tss_set_dpl1_stack(uint32_t esp, uint16_t ss) {
	tss.esp1 = esp;
	tss.ss1 = ss;
}

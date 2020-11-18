#include <i386/gdt.h>
#include <i386/tss.h>
#include <utils.h>

#define GDT_ENTRIES_COUNT 8

struct gdtr {
	uint16_t size;
	uint32_t offset;
} packed;

struct gdt_entry {
	uint16_t limit_low;
	uint32_t base_low : 24;
	uint8_t access;
	uint8_t granularity;
	uint8_t base_high;
} packed;

static struct gdt_entry gdt[GDT_ENTRIES_COUNT];
static struct gdtr gdt_pointer;

void gdt_install_entry(uint32_t index, uint32_t base, uint32_t limit,
					   uint8_t access, uint8_t granularity) {
	gdt[index].base_low = (base & 0xFFFFFF);
	gdt[index].base_high = (base >> 24) & 0xFF;

	gdt[index].limit_low = (limit & 0xFFFF);
	gdt[index].granularity = (limit >> 16) & 0x0F;

	gdt[index].granularity |= granularity & 0xF0;
	gdt[index].access = access;
}

uint16_t gdt_get_tss_segment() { return 7 * 8; }

void gdt_install_tss() {
	uint32_t base = tss_get_base();
	uint32_t limit = tss_get_limit();
	gdt_install_entry(7, base, limit, 0x89, 0x00);
}

void gdt_init() {
	gdt_install_entry(0, 0, 0, 0, 0);				 // null descriptor
	gdt_install_entry(1, 0, 0xFFFFFFFF, 0x9A, 0xCF); // scheduler code 0x08
	gdt_install_entry(2, 0, 0xFFFFFFFF, 0x92, 0xCF); // scheduler data 0x10
	gdt_install_entry(3, 0, 0xFFFFFFFF, 0xBA, 0xCF); // kernel code 0x18
	gdt_install_entry(4, 0, 0xFFFFFFFF, 0xB2, 0xCF); // kernel data 0x20
	gdt_install_entry(5, 0, 0xBFFFFFFF, 0xFA, 0xCF); // user code 0x28
	gdt_install_entry(6, 0, 0xBFFFFFFF, 0xF2, 0xCF); // user data 0x30
	gdt_install_tss();
	gdt_pointer.size = sizeof(gdt) - 1;
	gdt_pointer.offset = (uint32_t)&gdt;
	asm volatile("lgdt %0" : : "m"(gdt_pointer));
}

#include <i386/gdt.h>
#include <utils.h>

#define GDT_ENTRIES_COUNT 3

struct gdtr {
	uint16_t size;
	uint32_t offset;
} packed;

static uint64_t gdt_entries[] = {0x0000000000000000, 0x00CF9A000000FFFF,
                                 0x00CF92000000FFFF, 0x00CFBA000000FFFF,
                                 0x00CFB2000000FFFF, 0x00CBFA000000FFFF,
                                 0x00CBF2000000FFFF};

static struct gdtr gdt_pointer;

void gdt_init() {
	gdt_pointer.size = sizeof(gdt_entries) - 1;
	gdt_pointer.offset = (uint32_t)&gdt_entries;
	asm volatile("lgdt %0" : : "m"(gdt_pointer));
}
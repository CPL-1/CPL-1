#include <i386/idt.h>

struct idt_entry {
	uint16_t base_low;
	uint16_t selector;
	uint8_t reserved;
	uint8_t flags;
	uint16_t base_high;
} packed;

struct idtr {
	uint16_t limit;
	uint32_t base;
} packed;

static struct idtr idt_pointer;
static struct idt_entry idt_entries[256];

void idt_install_handler(uint8_t index, uint32_t entry, uint8_t flags) {
	idt_entries[index].base_low = (uint16_t)(entry & 0xffff);
	idt_entries[index].selector = 0x8;
	idt_entries[index].reserved = 0;
	idt_entries[index].flags = flags;
	idt_entries[index].base_high = (uint16_t)((entry >> 16) & 0xffff);
}

void idt_init() {
	memset(&idt_entries, 0, sizeof(idt_entries));
	idt_pointer.limit = sizeof(idt_entries) - 1;
	idt_pointer.base = (uint32_t)&idt_entries;
	asm volatile("lidt %0" : : "m"(idt_pointer));
}

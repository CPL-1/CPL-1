#include <arch/i386/idt.h>

struct i386_idt_entry {
	uint16_t base_low;
	uint16_t selector;
	uint8_t reserved;
	uint8_t flags;
	uint16_t base_high;
} packed;

struct i386_idtr {
	uint16_t limit;
	uint32_t base;
} packed;

static struct i386_idtr i386_idt_pointer;
static struct i386_idt_entry i386_idt_entries[256];

void i386_idt_install_handler(uint8_t index, uint32_t entry, uint8_t flags) {
	i386_idt_entries[index].base_low = (uint16_t)(entry & 0xffff);
	i386_idt_entries[index].selector = 0x8;
	i386_idt_entries[index].reserved = 0;
	i386_idt_entries[index].flags = flags;
	i386_idt_entries[index].base_high = (uint16_t)((entry >> 16) & 0xffff);
}

void i386_idt_init() {
	memset(&i386_idt_entries, 0, sizeof(i386_idt_entries));
	i386_idt_pointer.limit = sizeof(i386_idt_entries) - 1;
	i386_idt_pointer.base = (uint32_t)&i386_idt_entries;
	asm volatile("lidt %0" : : "m"(i386_idt_pointer));
}

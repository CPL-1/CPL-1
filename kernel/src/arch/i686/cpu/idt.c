#include <arch/i686/cpu/idt.h>

struct i686_idt_entry {
	uint16_t base_low;
	uint16_t selector;
	uint8_t reserved;
	uint8_t flags;
	uint16_t base_high;
} packed;

struct i686_idtr {
	uint16_t limit;
	uint32_t base;
} packed;

static struct i686_idtr i686_idt_pointer;
static struct i686_idt_entry i686_idt_entries[256];

void i686_idt_install_handler(uint8_t index, uint32_t entry, uint8_t flags) {
	i686_idt_entries[index].base_low = (uint16_t)(entry & 0xffff);
	i686_idt_entries[index].selector = 0x8;
	i686_idt_entries[index].reserved = 0;
	i686_idt_entries[index].flags = flags;
	i686_idt_entries[index].base_high = (uint16_t)((entry >> 16) & 0xffff);
}

void i686_idt_init() {
	memset(&i686_idt_entries, 0, sizeof(i686_idt_entries));
	i686_idt_pointer.limit = sizeof(i686_idt_entries) - 1;
	i686_idt_pointer.base = (uint32_t)&i686_idt_entries;
	asm volatile("lidt %0" : : "m"(i686_idt_pointer));
}

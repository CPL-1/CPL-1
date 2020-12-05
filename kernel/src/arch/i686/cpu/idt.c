#include <arch/i686/cpu/idt.h>

struct i686_IDT_Entry {
	uint16_t base_low;
	uint16_t selector;
	uint8_t reserved;
	uint8_t flags;
	uint16_t base_high;
} PACKED;

struct i686_IDTR {
	uint16_t limit;
	uint32_t base;
} PACKED;

static struct i686_IDTR i686_IDT_Pointer;
static struct i686_IDT_Entry i686_IDT_Entries[256];

void i686_IDT_InstallHandler(uint8_t index, uint32_t entry, uint8_t flags) {
	i686_IDT_Entries[index].base_low = (uint16_t)(entry & 0xffff);
	i686_IDT_Entries[index].selector = 0x8;
	i686_IDT_Entries[index].reserved = 0;
	i686_IDT_Entries[index].flags = flags;
	i686_IDT_Entries[index].base_high = (uint16_t)((entry >> 16) & 0xffff);
}

void i686_IDT_Initialize() {
	memset(&i686_IDT_Entries, 0, sizeof(i686_IDT_Entries));
	i686_IDT_Pointer.limit = sizeof(i686_IDT_Entries) - 1;
	i686_IDT_Pointer.base = (uint32_t)&i686_IDT_Entries;
	ASM volatile("lidt %0" : : "m"(i686_IDT_Pointer));
}

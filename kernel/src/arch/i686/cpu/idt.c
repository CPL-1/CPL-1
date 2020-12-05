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

static struct i686_IDTR m_IDTPointer;
static struct i686_IDT_Entry m_IDTEntries[256];

void i686_IDT_InstallHandler(uint8_t index, uint32_t entry, uint8_t flags) {
	m_IDTEntries[index].base_low = (uint16_t)(entry & 0xffff);
	m_IDTEntries[index].selector = 0x8;
	m_IDTEntries[index].reserved = 0;
	m_IDTEntries[index].flags = flags;
	m_IDTEntries[index].base_high = (uint16_t)((entry >> 16) & 0xffff);
}

void i686_IDT_Initialize() {
	memset(&m_IDTEntries, 0, sizeof(m_IDTEntries));
	m_IDTPointer.limit = sizeof(m_IDTEntries) - 1;
	m_IDTPointer.base = (uint32_t)&m_IDTEntries;
	ASM volatile("lidt %0" : : "m"(m_IDTPointer));
}

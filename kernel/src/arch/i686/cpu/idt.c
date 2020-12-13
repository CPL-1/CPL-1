#include <arch/i686/cpu/idt.h>

struct i686_IDT_Entry {
	uint16_t baseLow;
	uint16_t selector;
	uint8_t reserved;
	struct {
		uint8_t type : 4;
		uint8_t storageSegment : 1;
		uint8_t dpl : 2;
		uint8_t present : 1;
	} PACKED;
	uint16_t baseHigh;
} PACKED;

struct i686_IDTR {
	uint16_t limit;
	uint32_t base;
} PACKED;

static struct i686_IDTR m_IDTPointer;
static struct i686_IDT_Entry m_IDTEntries[256];

void i686_IDT_InstallHandler(uint8_t index, uint32_t entry, uint8_t gateType, uint8_t entryDPL, uint8_t selector) {
	m_IDTEntries[index].baseLow = (uint16_t)(entry & 0xffff);
	m_IDTEntries[index].selector = selector;
	m_IDTEntries[index].reserved = 0;
	m_IDTEntries[index].storageSegment = 0;
	m_IDTEntries[index].type = gateType;
	m_IDTEntries[index].dpl = entryDPL;
	m_IDTEntries[index].present = 1;
	m_IDTEntries[index].baseHigh = (uint16_t)((entry >> 16) & 0xffff);
}

void i686_IDT_Initialize() {
	memset(&m_IDTEntries, 0, sizeof(m_IDTEntries));
	m_IDTPointer.limit = sizeof(m_IDTEntries) - 1;
	m_IDTPointer.base = (uint32_t)&m_IDTEntries;
	ASM VOLATILE("lidt %0" : : "m"(m_IDTPointer));
}

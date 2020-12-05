#include <arch/i686/cpu/gdt.h>
#include <arch/i686/cpu/tss.h>
#include <common/misc/utils.h>

#define GDT_ENTRIES_COUNT 8

struct i686_GDTR {
	uint16_t size;
	uint32_t offset;
} PACKED;

struct i686_GDT_Entry {
	uint16_t limitLow;
	uint32_t baseLow : 24;
	uint8_t access;
	uint8_t granularity;
	uint8_t baseHigh;
} PACKED;

static struct i686_GDT_Entry m_entries[GDT_ENTRIES_COUNT];
static struct i686_GDTR m_GDTPointer;

void i686_GDT_InstallEntry(uint32_t index, uint32_t base, uint32_t limit, uint8_t access, uint8_t granularity) {
	m_entries[index].baseLow = (base & 0xFFFFFF);
	m_entries[index].baseHigh = (base >> 24) & 0xFF;

	m_entries[index].limitLow = (limit & 0xFFFF);
	m_entries[index].granularity = (limit >> 16) & 0x0F;

	m_entries[index].granularity |= granularity & 0xF0;
	m_entries[index].access = access;
}

uint16_t i686_GDT_GetTSSSegment() {
	return 7 * 8;
}

void i686_GDT_InstallTSS() {
	uint32_t base = i686_TSS_GetBase();
	uint32_t limit = i686_TSS_GetLimit();
	i686_GDT_InstallEntry(7, base, limit, 0x89, 0x00);
}

void i686_GDT_Initialize() {
	i686_GDT_InstallEntry(0, 0, 0, 0, 0);				 // null descriptor
	i686_GDT_InstallEntry(1, 0, 0xFFFFFFFF, 0x9A, 0xCF); // scheduler code 0x08
	i686_GDT_InstallEntry(2, 0, 0xFFFFFFFF, 0x92, 0xCF); // scheduler data 0x10
	i686_GDT_InstallEntry(3, 0, 0xFFFFFFFF, 0xBA, 0xCF); // kernel code 0x18
	i686_GDT_InstallEntry(4, 0, 0xFFFFFFFF, 0xB2, 0xCF); // kernel data 0x20
	i686_GDT_InstallEntry(5, 0, 0xBFFFFFFF, 0xFA, 0xCF); // user code 0x28
	i686_GDT_InstallEntry(6, 0, 0xBFFFFFFF, 0xF2, 0xCF); // user data 0x30
	i686_GDT_InstallTSS();
	m_GDTPointer.size = sizeof(m_entries) - 1;
	m_GDTPointer.offset = (uint32_t)&m_entries;
	ASM volatile("lgdt %0" : : "m"(m_GDTPointer));
}

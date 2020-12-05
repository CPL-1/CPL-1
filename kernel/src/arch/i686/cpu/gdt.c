#include <arch/i686/cpu/gdt.h>
#include <arch/i686/cpu/tss.h>
#include <common/misc/utils.h>

#define GDT_ENTRIES_COUNT 8

struct i686_GDTR {
	UINT16 size;
	UINT32 offset;
} PACKED;

struct i686_GDT_Entry {
	UINT16 limitLow;
	UINT32 baseLow : 24;
	UINT8 access;
	UINT8 granularity;
	UINT8 baseHigh;
} PACKED;

static struct i686_GDT_Entry i686_GDT_Entries[GDT_ENTRIES_COUNT];
static struct i686_GDTR i686_GDT_Pointer;

void i686_GDT_InstallEntry(UINT32 index, UINT32 base, UINT32 limit, UINT8 access, UINT8 granularity) {
	i686_GDT_Entries[index].baseLow = (base & 0xFFFFFF);
	i686_GDT_Entries[index].baseHigh = (base >> 24) & 0xFF;

	i686_GDT_Entries[index].limitLow = (limit & 0xFFFF);
	i686_GDT_Entries[index].granularity = (limit >> 16) & 0x0F;

	i686_GDT_Entries[index].granularity |= granularity & 0xF0;
	i686_GDT_Entries[index].access = access;
}

UINT16 i686_GDT_GetTSSSegment() {
	return 7 * 8;
}

void i686_GDT_InstallTSS() {
	UINT32 base = i686_TSS_GetBase();
	UINT32 limit = i686_TSS_GetLimit();
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
	i686_GDT_Pointer.size = sizeof(i686_GDT_Entries) - 1;
	i686_GDT_Pointer.offset = (UINT32)&i686_GDT_Entries;
	ASM volatile("lgdt %0" : : "m"(i686_GDT_Pointer));
}

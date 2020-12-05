#include <arch/i686/cpu/idt.h>

struct i686_IDT_Entry {
	UINT16 base_low;
	UINT16 selector;
	UINT8 reserved;
	UINT8 flags;
	UINT16 base_high;
} PACKED;

struct i686_IDTR {
	UINT16 limit;
	UINT32 base;
} PACKED;

static struct i686_IDTR i686_IDT_Pointer;
static struct i686_IDT_Entry i686_IDT_Entries[256];

void i686_IDT_InstallHandler(UINT8 index, UINT32 entry, UINT8 flags) {
	i686_IDT_Entries[index].base_low = (UINT16)(entry & 0xffff);
	i686_IDT_Entries[index].selector = 0x8;
	i686_IDT_Entries[index].reserved = 0;
	i686_IDT_Entries[index].flags = flags;
	i686_IDT_Entries[index].base_high = (UINT16)((entry >> 16) & 0xffff);
}

void i686_IDT_Initialize() {
	memset(&i686_IDT_Entries, 0, sizeof(i686_IDT_Entries));
	i686_IDT_Pointer.limit = sizeof(i686_IDT_Entries) - 1;
	i686_IDT_Pointer.base = (UINT32)&i686_IDT_Entries;
	ASM volatile("lidt %0" : : "m"(i686_IDT_Pointer));
}

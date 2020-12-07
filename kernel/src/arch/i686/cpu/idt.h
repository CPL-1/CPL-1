#ifndef __I686_IDT_H_INCLUDED__
#define __I686_IDT_H_INCLUDED__

#include <common/misc/utils.h>

#define I686_32BIT_TASK_GATE 0x5
#define I686_32BIT_INT_GATE 0xe
#define I686_32BIT_TRAP_GATE 0xf

void i686_IDT_Initialize();
void i686_IDT_InstallHandler(uint8_t index, uint32_t entry, uint8_t gateType, uint8_t maxEntryDPL, uint8_t selector);

static INLINE void i686_IDT_InstallISR(uint8_t index, uint32_t entry) {
	i686_IDT_InstallHandler(index, entry, I686_32BIT_INT_GATE, 1, 0x08);
}

#endif

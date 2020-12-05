#ifndef __I686_IDT_H_INCLUDED__
#define __I686_IDT_H_INCLUDED__

#include <common/misc/utils.h>

#define I686_ISR_FLAGS 0xAE

void i686_IDT_Initialize();
void i686_IDT_InstallHandler(uint8_t index, uint32_t entry, uint8_t flags);

static INLINE void i686_IDT_InstallISR(uint8_t index, uint32_t entry) {
	i686_IDT_InstallHandler(index, entry, I686_ISR_FLAGS);
}

#endif

#ifndef __IDT_H_INCLUDED__
#define __IDT_H_INCLUDED__

#include <utils.h>

// We use 0xAE (DPL = 1) so that kernel can emulate hardware interrupts
#define ISR_FLAGS 0xAE

void idt_init();
void idt_install_handler(uint8_t index, uint32_t entry, uint8_t flags);

inline static void idt_install_isr(uint8_t index, uint32_t entry) {
	idt_install_handler(index, entry, ISR_FLAGS);
}

#endif

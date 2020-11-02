#ifndef __IDT_H_INCLUDED__
#define __IDT_H_INCLUDED__

#include <utils.h>

#define ISR_FLAGS 0x8E

void idt_init();
void idt_install_handler(uint8_t index, uint32_t entry, uint8_t flags);

inline static void idt_install_isr(uint8_t index, uint32_t entry) {
	idt_install_handler(index, entry, ISR_FLAGS);
}

#endif
#ifndef __I386_IDT_H_INCLUDED__
#define __I386_IDT_H_INCLUDED__

#include <utils/utils.h>

#define ISR_FLAGS 0xAE

void idt_init();
void idt_install_handler(uint8_t index, uint32_t entry, uint8_t flags);

static inline void idt_install_isr(uint8_t index, uint32_t entry) {
	idt_install_handler(index, entry, ISR_FLAGS);
}

#endif

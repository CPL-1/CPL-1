#ifndef __I386_IDT_H_INCLUDED__
#define __I386_IDT_H_INCLUDED__

#include <common/misc/utils.h>

#define ISR_FLAGS 0xAE

void i386_idt_init();
void i386_idt_install_handler(uint8_t index, uint32_t entry, uint8_t flags);

static inline void i386_idt_install_isr(uint8_t index, uint32_t entry) {
	i386_idt_install_handler(index, entry, ISR_FLAGS);
}

#endif

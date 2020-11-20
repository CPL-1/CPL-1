#include <arch/i686/cpu/idt.h>
#include <arch/i686/proc/priv.h>

extern void i686_priv_call_ring0_isr();

void i686_priv_init() {
	i686_idt_install_isr(0xff, (uint32_t)i686_priv_call_ring0_isr);
}
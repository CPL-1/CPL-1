#include <arch/i386/idt.h>
#include <arch/i386/proc/priv.h>

extern void priv_call_ring0_isr();

void priv_init() { i386_idt_install_isr(0xff, (uint32_t)priv_call_ring0_isr); }
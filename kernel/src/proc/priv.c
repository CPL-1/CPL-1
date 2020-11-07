#include <i386/idt.h>
#include <proc/priv.h>

extern void priv_call_ring0_isr();

void priv_init() { idt_install_isr(0xff, (uint32_t)priv_call_ring0_isr); }
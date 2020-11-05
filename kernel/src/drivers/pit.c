#include <drivers/pic.h>
#include <drivers/pit.h>
#include <drivers/ports.h>
#include <i386/idt.h>

extern void pit_interrupt_handler();

static uint32_t pit_handler = 0;

void pit_call_handler(uint32_t frame) {
	if (pit_handler != 0) {
		((void (*)(uint32_t))pit_handler)(frame);
	}
	pic_irq_notify_on_term(0);
}

void pit_init(uint32_t freq) {
	uint32_t divisor = 1193180 / freq;
	outb(0x43, 0x36);

	uint8_t lo = (uint8_t)(divisor & 0xff);
	uint8_t hi = (uint8_t)((divisor >> 8) & 0xff);

	outb(0x40, lo);
	outb(0x40, hi);

	idt_install_isr(0x20, (uint32_t)pit_interrupt_handler);
}

void pit_set_callback(uint32_t entry) {
	pit_handler = entry;
	pic_irq_enable(0);
}

void pit_trigger_interrupt() { asm volatile("int $0x20"); }

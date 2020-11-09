#include <drivers/pic.h>
#include <drivers/pit.h>
#include <fembed.h>
#include <i386/idt.h>
#include <i386/ports.h>
#include <proc/iowait.h>

void pit_init(uint32_t freq) {
	uint32_t divisor = 1193180 / freq;
	outb(0x43, 0x36);

	uint8_t lo = (uint8_t)(divisor & 0xff);
	uint8_t hi = (uint8_t)((divisor >> 8) & 0xff);

	outb(0x40, lo);
	outb(0x40, hi);
}

void pit_trigger_interrupt() { asm volatile("int $0x20"); }

void pit_set_callback(uint32_t entry) {
	iowait_add_handler(0, (iowait_handler_t)entry, NULL, NULL);
}

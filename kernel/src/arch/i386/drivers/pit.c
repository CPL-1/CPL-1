#include <arch/i386/drivers/pic.h>
#include <arch/i386/drivers/pit.h>
#include <arch/i386/idt.h>
#include <arch/i386/ports.h>
#include <arch/i386/proc/iowait.h>
#include <arch/i386/proc/isrhandler.h>
#include <hal/proc/isrhandler.h>

void i386_pit_init(uint32_t freq) {
	uint32_t divisor = 1193180 / freq;
	outb(0x43, 0x36);

	uint8_t lo = (uint8_t)(divisor & 0xff);
	uint8_t hi = (uint8_t)((divisor >> 8) & 0xff);

	outb(0x40, lo);
	outb(0x40, hi);
}

bool hal_timer_set_callback(hal_isr_handler_t entry) {
	iowait_add_handler(0, (iowait_handler_t)entry, NULL, NULL);
	hal_isr_handler_t handler = i386_isr_handler_new(entry, NULL);
	if (handler == NULL) {
		return false;
	}
	i386_idt_install_isr(0xfe, (uint32_t)handler);
	return true;
}

void hal_timer_trigger_callback() { asm volatile("int $0xfe"); }
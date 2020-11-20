#include <arch/i686/cpu/idt.h>
#include <arch/i686/cpu/ports.h>
#include <arch/i686/drivers/pic.h>
#include <arch/i686/drivers/pit.h>
#include <arch/i686/proc/iowait.h>
#include <arch/i686/proc/isrhandler.h>
#include <hal/proc/isrhandler.h>

void i686_pit_init(uint32_t freq) {
	uint32_t divisor = 1193180 / freq;
	outb(0x43, 0x36);

	uint8_t lo = (uint8_t)(divisor & 0xff);
	uint8_t hi = (uint8_t)((divisor >> 8) & 0xff);

	outb(0x40, lo);
	outb(0x40, hi);
}

bool hal_timer_set_callback(hal_isr_handler_t entry) {
	i686_iowait_add_handler(0, (i686_iowait_handler_t)entry, NULL, NULL);
	hal_isr_handler_t handler = i686_isr_handler_new(entry, NULL);
	if (handler == NULL) {
		return false;
	}
	i686_idt_install_isr(0xfe, (uint32_t)handler);
	return true;
}

void hal_timer_trigger_callback() { asm volatile("int $0xfe"); }
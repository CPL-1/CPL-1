#include <arch/i686/cpu/ports.h>
#include <arch/i686/drivers/tty.h>
#include <hal/drivers/tty.h>
#include <hal/memory/virt.h>

void i686_tty_init() {}

void hal_tty_flush() {}

void hal_tty_putc(char c) { outb(0xe9, c); }

void hal_tty_set_color(unused uint8_t color) {}

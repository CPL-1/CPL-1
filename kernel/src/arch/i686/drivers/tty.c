#include <arch/i686/cpu/ports.h>
#include <arch/i686/drivers/tty.h>
#include <hal/drivers/tty.h>
#include <hal/memory/virt.h>

static const uintptr_t TEXT_VGA_MEMORY = 0xc00b8000;
static const uint16_t VGA_HEIGHT = 25;
static const uint16_t VGA_WIDTH = 80;
static const uint16_t TAB_SIZE = 4;
static uint8_t i686_tty_color = 0x07;

static uint16_t i686_tty_add_color(char character) {
	uint16_t word_char = (uint16_t)character;
	uint16_t word_color = (uint16_t)i686_tty_color;
	return (word_color << 8) + word_char;
}

static void i686_tty_putc_raw_at(uint16_t x, uint16_t y, char c) {
	uint16_t combined = i686_tty_add_color(c);
	uint16_t screen_coord = y * VGA_WIDTH + x;
	((volatile uint16_t *)TEXT_VGA_MEMORY)[screen_coord] = combined;
}

static uint16_t i686_tty_x, i686_tty_y;

static void i686_tty_scroll() {
	void *i686_tty_first_line = (void *)(TEXT_VGA_MEMORY);
	const void *i686_tty_second_line =
		(const void *)(TEXT_VGA_MEMORY + 2 * VGA_WIDTH);
	memcpy(i686_tty_first_line, i686_tty_second_line,
		   2 * VGA_WIDTH * (VGA_HEIGHT - 1));
	for (size_t x = 0; x < VGA_WIDTH; ++x) {
		i686_tty_putc_raw_at(x, VGA_HEIGHT - 1, ' ');
	}
}

static void i686_tty_putc_raw(char c) {
	if (i686_tty_y >= VGA_HEIGHT) {
		i686_tty_y = VGA_HEIGHT - 1;
		i686_tty_x = 0;
		i686_tty_scroll();
	}
	i686_tty_putc_raw_at(i686_tty_x, i686_tty_y, c);
	if (++i686_tty_x >= VGA_WIDTH) {
		i686_tty_x = 0;
		++i686_tty_y;
	}
}

static void i686_tty_newline() {
	if (i686_tty_x == 0) {
		i686_tty_putc_raw(' ');
	}
	while (i686_tty_x != 0) {
		i686_tty_putc_raw(' ');
	}
}

static void i686_tty_tab() {
	if (i686_tty_x % TAB_SIZE == 0) {
		i686_tty_putc_raw(' ');
	}
	while (i686_tty_x % TAB_SIZE != 0) {
		i686_tty_putc_raw(' ');
	}
}

void hal_tty_clear() {
	for (size_t y = 0; y < VGA_HEIGHT; ++y) {
		for (size_t x = 0; x < VGA_WIDTH; ++x) {
			i686_tty_putc_raw_at(x, y, ' ');
		}
	}
	i686_tty_x = 0;
	i686_tty_y = 0;
	hal_tty_flush();
}

static void i686_tty_enable_cursor() {
	outb(0x3d4, 0x0a);
	outb(0x3d5, (inb(0x3d5) & 0xc0) | 13);

	outb(0x3d4, 0x0b);
	outb(0x3d5, (inb(0x3d5) & 0xe0) | 15);
}

void i686_tty_init() {
	i686_tty_enable_cursor();
	hal_tty_clear();
}

void hal_tty_flush() {
	uint16_t pos = i686_tty_y * VGA_WIDTH + i686_tty_x;

	outb(0x3d4, 0x0f);
	outb(0x3d5, (uint8_t)(pos & 0xff));

	outb(0x3d4, 0x0e);
	outb(0x3d5, (uint8_t)((pos >> 8) & 0xff));
}

void hal_tty_putc(char c) {
	if (c == '\n') {
		i686_tty_newline();
	} else if (c == '\t') {
		i686_tty_tab();
	} else {
		i686_tty_putc_raw(c);
	}
	outb(0xe9, c);
}

void hal_tty_set_color(uint8_t color) { i686_tty_color = color; }
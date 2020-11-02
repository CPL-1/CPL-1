#include <drivers/ports.h>
#include <drivers/textvga.h>

const static uintptr_t TEXT_VGA_MEMORY = 0xc00b8000;
const static uint16_t VGA_HEIGHT = 25;
const static uint16_t VGA_WIDTH = 80;
const static uint16_t TAB_SIZE = 4;
static uint8_t vga_color = 0x07;

static uint16_t vga_add_color(char character) {
	uint16_t word_char = (uint16_t)character;
	uint16_t word_color = (uint16_t)vga_color;
	return (word_color << 8) + word_char;
}

static void vga_putc_raw_at(uint16_t x, uint16_t y, char c) {
	uint16_t combined = vga_add_color(c);
	uint16_t screen_coord = y * VGA_WIDTH + x;
	((volatile uint16_t *)TEXT_VGA_MEMORY)[screen_coord] = combined;
}

static uint16_t vga_x, vga_y;

static void vga_scroll() {
	void *vga_first_line = (void *)(TEXT_VGA_MEMORY);
	const void *vga_second_line =
	    (const void *)(TEXT_VGA_MEMORY + 2 * VGA_WIDTH);
	memcpy(vga_first_line, vga_second_line, 2 * VGA_WIDTH * (VGA_HEIGHT - 1));
	for (size_t x = 0; x < VGA_WIDTH; ++x) {
		vga_putc_raw_at(x, VGA_HEIGHT - 1, ' ');
	}
}

static void vga_putc_raw(char c) {
	if (vga_y >= VGA_HEIGHT) {
		vga_y = VGA_HEIGHT - 1;
		vga_x = 0;
		vga_scroll();
	}
	vga_putc_raw_at(vga_x, vga_y, c);
	if (++vga_x >= VGA_WIDTH) {
		vga_x = 0;
		++vga_y;
	}
}

static void vga_newline() {
	if (vga_x == 0) {
		vga_putc_raw(' ');
	}
	while (vga_x != 0) {
		vga_putc_raw(' ');
	}
}

static void vga_tab() {
	if (vga_x % TAB_SIZE == 0) {
		vga_putc_raw(' ');
	}
	while (vga_x % TAB_SIZE != 0) {
		vga_putc_raw(' ');
	}
}

void vga_clear_screen() {
	for (size_t y = 0; y < VGA_HEIGHT; ++y) {
		for (size_t x = 0; x < VGA_WIDTH; ++x) {
			vga_putc_raw_at(x, y, ' ');
		}
	}
	vga_x = 0;
	vga_y = 0;
	vga_update_cursor();
}

static void vga_enable_cursor() {
	outb(0x3d4, 0x0a);
	outb(0x3d5, (inb(0x3d5) & 0xc0) | 13);

	outb(0x3d4, 0x0b);
	outb(0x3d5, (inb(0x3d5) & 0xe0) | 15);
}

void vga_init() {
	vga_enable_cursor();
	vga_clear_screen();
}

void vga_update_cursor() {
	uint16_t pos = vga_y * VGA_WIDTH + vga_x;

	outb(0x3d4, 0x0f);
	outb(0x3d5, (uint8_t)(pos & 0xff));

	outb(0x3d4, 0x0e);
	outb(0x3d5, (uint8_t)((pos >> 8) & 0xff));
}

void vga_putc_no_cursor_update(char c) {
	if (c == '\n') {
		vga_newline();
	} else if (c == '\t') {
		vga_tab();
	} else {
		vga_putc_raw(c);
	}
}

void vga_puts(const char *str) {
	char c;
	while ((c = *str++) != '\0') {
		vga_putc_no_cursor_update(c);
	}
	vga_update_cursor();
}

void vga_set_color(uint8_t color) { vga_color = color; }
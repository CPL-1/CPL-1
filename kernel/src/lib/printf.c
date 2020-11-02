#include <drivers/textvga.h>
#include <lib/printf.h>

char char_from_digit(uint8_t digit) {
	if (digit >= 10) {
		return 'a' - 10 + digit;
	}
	return '0' + digit;
}

void putui(uint32_t val, uint8_t base, bool rec) {
	if (val == 0 && rec) {
		return;
	}
	putui(val / base, base, true);
	vga_putc_no_cursor_update(char_from_digit(val % base));
}

void puti(int32_t val, int8_t base) {
	if (val < 0) {
		vga_putc_no_cursor_update('-');
		putui((uint64_t)-val, base, false);
	} else {
		putui((uint64_t)val, base, false);
	}
}

void puts(const char *str) {
	for (uint64_t i = 0; str[i] != '\0'; ++i) {
		vga_putc_no_cursor_update(str[i]);
	}
}

void putp(uintptr_t pointer, int depth) {
	if (depth == 0) {
		return;
	}
	putp(pointer / 16, depth - 1);
	vga_putc_no_cursor_update(char_from_digit(pointer % 16));
}

void printf(const char *fmt, ...) {
	va_list args;
	va_start(args, fmt);
	va_printf(fmt, args);
	va_end(args);
}

void va_printf(const char *fmt, va_list args) {
	for (uint64_t i = 0; fmt[i] != '\0'; ++i) {
		if (fmt[i] != '%') {
			vga_putc_no_cursor_update(fmt[i]);
		} else {
			++i;
			switch (fmt[i]) {
				case '%':
					vga_putc_no_cursor_update('%');
					break;
				case 'd':
					puti(va_arg(args, int32_t), 10);
					break;
				case 'u':
					putui(va_arg(args, uint32_t), 10, false);
					break;
				case 'p':
					putp(va_arg(args, uintptr_t), 16);
					break;
				case 's':
					puts(va_arg(args, char *));
					break;
				case 'c':
					vga_putc_no_cursor_update((char)va_arg(args, int));
					break;
				case 'l':
					++i;
					switch (fmt[i]) {
						case 'u':
							putui(va_arg(args, uint32_t), 10, false);
							break;
						case 'd':
							puti(va_arg(args, int32_t), 10);
							break;
						default:
							break;
					}
				default:
					break;
			}
		}
	}
	vga_update_cursor();
}

void write(const char *str, uint64_t size) {
	for (uint64_t i = 0; i < size; ++i) {
		vga_putc_no_cursor_update(str[i]);
	}
}
#include <arch/i386/ports.h>
#include <hal/drivers/tty.h>
#include <lib/printf.h>

char char_from_digit(uint8_t digit) {
	if (digit >= 10) {
		return 'a' - 10 + digit;
	}
	return '0' + digit;
}

size_t putui(uint64_t val, uint8_t base, bool rec, char *buf, size_t size) {
	if (val == 0 && rec) {
		return 0;
	}
	size_t used = putui(val / base, base, true, buf, size);
	if (used >= size) {
		return used;
	}
	buf[used] = char_from_digit(val % base);
	return used + 1;
}

size_t puti(int64_t val, int8_t base, char *buf, size_t size) {
	if (size == 0) {
		return 0;
	}
	if (val < 0) {
		buf[0] = '-';
		return putui((uint64_t)-val, base, false, buf + 1, size - 1) + 1;
	}
	return putui((uint64_t)val, base, false, buf, size);
}

size_t puts(const char *str, char *buf, size_t size) {
	size_t pos = 0;
	for (uint64_t i = 0; str[i] != '\0'; ++i) {
		if (pos >= size) {
			return pos;
		}
		buf[pos] = str[i];
		++pos;
	}
	return pos;
}

size_t putp(uintptr_t pointer, int depth, char *buf, size_t size) {
	if (depth == 0) {
		return 0;
	}
	if (size == 0) {
		return 0;
	}
	size_t used = putp(pointer / 16, depth - 1, buf, size);
	if (size == used) {
		return used;
	}
	buf[used] = char_from_digit(pointer % 16);
	return used + 1;
}

size_t printf(const char *fmt, ...) {
	va_list args;
	va_start(args, fmt);
	size_t result = va_printf(fmt, args);
	va_end(args);
	return result;
}

size_t sprintf(const char *fmt, char *buf, size_t size, ...) {
	va_list args;
	va_start(args, size);
	size_t result = va_sprintf(fmt, buf, size, args);
	va_end(args);
	return result;
}

size_t va_sprintf(const char *fmt, char *buf, size_t size, va_list args) {
	size_t pos = 0;
	for (int i = 0; fmt[i] != '\0'; ++i) {
		if (pos >= size) {
			return size;
		}
		if (fmt[i] != '%') {
			buf[pos] = fmt[i];
			++pos;
		} else {
			++i;
			switch (fmt[i]) {
			case '%':
				buf[pos] = '%';
				pos++;
				break;
			case 'd':
				pos += puti(va_arg(args, int32_t), 10, buf + pos, size - pos);
				break;
			case 'u':
				pos += putui(va_arg(args, uint32_t), 10, false, buf + pos,
							 size - pos);
				break;
			case 'p':
				pos += putp(va_arg(args, uintptr_t), 8, buf + pos, size - pos);
				break;
			case 's':
				pos += puts(va_arg(args, char *), buf + pos, size - pos);
				break;
			case 'c':
				buf[pos] = (char)va_arg(args, int);
				++pos;
				break;
			case 'l':
				++i;
				switch (fmt[i]) {
				case 'u':
					pos += putui(va_arg(args, uint64_t), 10, false, buf + pos,
								 size - pos);
					break;
				case 'd':
					pos +=
						puti(va_arg(args, int64_t), 10, buf + pos, size - pos);
					break;
				default:
					break;
				}
			default:
				break;
			}
		}
	}
	return pos;
}

void write(const char *str, uint64_t size) {
	for (uint64_t i = 0; i < size; ++i) {
		hal_tty_putc(str[i]);
		outb(0xe9, str[i]);
	}
	hal_tty_flush();
}

size_t va_printf(const char *str, va_list args) {
	char buf[1024];
	size_t count = va_sprintf(str, buf, 1024, args);
	write(buf, count);
	return count;
}
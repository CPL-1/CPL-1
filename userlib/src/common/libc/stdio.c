#include <errno.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/syscall.h>

static char __Printf_GetCharFromDigit(uint8_t digit) {
	if (digit >= 10) {
		return 'a' - 10 + digit;
	}
	return '0' + digit;
}

static int __Printf_PrintUnsignedInteger(uint64_t val, uint8_t base, bool rec, char *buf, int size) {
	if (val == 0 && rec) {
		return 0;
	}
	int used = __Printf_PrintUnsignedInteger(val / base, base, true, buf, size);
	if (used >= size) {
		return used;
	}
	buf[used] = __Printf_GetCharFromDigit(val % base);
	return used + 1;
}

static int __Printf_PrintInteger(int64_t val, int8_t base, char *buf, int size) {
	if (size == 0) {
		return 0;
	}
	if (val < 0) {
		buf[0] = '-';
		return __Printf_PrintUnsignedInteger((uint64_t)-val, base, false, buf + 1, size - 1) + 1;
	}
	return __Printf_PrintUnsignedInteger((uint64_t)val, base, false, buf, size);
}

static int __Printf_PrintString(const char *str, char *buf, int size) {
	int pos = 0;
	for (int i = 0; str[i] != '\0'; ++i) {
		if (pos >= size) {
			return pos;
		}
		buf[pos] = str[i];
		++pos;
	}
	return pos;
}

static int __Printf_PrintPointer(uintptr_t pointer, int depth, char *buf, int size) {
	if (depth == 0) {
		return 0;
	}
	if (size == 0) {
		return 0;
	}
	int used = __Printf_PrintPointer(pointer / 16, depth - 1, buf, size);
	if (size == used) {
		return used;
	}
	buf[used] = __Printf_GetCharFromDigit(pointer % 16);
	return used + 1;
}

int printf(const char *fmt, ...) {
	va_list args;
	va_start(args, fmt);
	int result = va_printf(fmt, args);
	va_end(args);
	return result;
}

int snprintf(char *buf, int size, const char *fmt, ...) {
	va_list args;
	va_start(args, fmt);
	int result = va_snprintf(buf, size, fmt, args);
	va_end(args);
	return result;
}

int va_snprintf(char *buf, int size, const char *fmt, va_list args) {
	if (size == 0) {
		return 0;
	}
	size -= 1;
	int pos = 0;
	for (int i = 0; fmt[i] != '\0'; ++i) {
		if (pos >= size) {
			buf[size] = '\0';
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
				pos += __Printf_PrintInteger((int64_t)va_arg(args, int32_t), 10, buf + pos, size - pos);
				break;
			case 'u':
				pos +=
					__Printf_PrintUnsignedInteger((uint64_t)va_arg(args, uint32_t), 10, false, buf + pos, size - pos);
				break;
			case 'p':
				pos += __Printf_PrintPointer((uint64_t)va_arg(args, uintptr_t), sizeof(uintptr_t) * 2, buf + pos,
											 size - pos);
				break;
			case 's':
				pos += __Printf_PrintString(va_arg(args, char *), buf + pos, size - pos);
				break;
			case 'c':
				buf[pos] = (char)va_arg(args, int);
				++pos;
				break;
			case 'l':
				++i;
				switch (fmt[i]) {
				case 'u':
					pos += __Printf_PrintUnsignedInteger(va_arg(args, uint64_t), 10, false, buf + pos, size - pos);
					break;
				case 'd':
					pos += __Printf_PrintInteger(va_arg(args, int64_t), 10, buf + pos, size - pos);
					break;
				default:
					break;
				}
			default:
				break;
			}
		}
	}
	buf[pos] = '\0';
	return pos;
}

void __Printf_WriteString(const char *str, int size) {
	write(1, str, size);
}

int puts(const char *str) {
	return write(1, str, strlen(str));
}

#define PRINTF_BUFFER_SIZE 65536

int va_printf(const char *str, va_list args) {
	char buf[PRINTF_BUFFER_SIZE];
	int count = va_snprintf(buf, PRINTF_BUFFER_SIZE, str, args);
	__Printf_WriteString(buf, count);
	return count;
}

int getchar() {
	char buf[1];
	int result = read(0, buf, 1);
	if (result <= 0) {
		return EOF;
	}
	return buf[0];
}

int putchar(int c) {
	char buf[1];
	buf[0] = (char)c;
	int result = write(1, buf, 1);
	if (result <= 0) {
		return EOF;
	}
	return c;
}
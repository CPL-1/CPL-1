#include <common/lib/printf.h>
#include <hal/drivers/tty.h>

static char printf_GetCharFromDigit(UINT8 digit) {
	if (digit >= 10) {
		return 'a' - 10 + digit;
	}
	return '0' + digit;
}

static USIZE printf_PrintUnsignedInteger(UINT64 val, UINT8 base, bool rec, char *buf, USIZE size) {
	if (val == 0 && rec) {
		return 0;
	}
	USIZE used = printf_PrintUnsignedInteger(val / base, base, true, buf, size);
	if (used >= size) {
		return used;
	}
	buf[used] = printf_GetCharFromDigit(val % base);
	return used + 1;
}

static USIZE printf_PrintInteger(INT64 val, INT8 base, char *buf, USIZE size) {
	if (size == 0) {
		return 0;
	}
	if (val < 0) {
		buf[0] = '-';
		return printf_PrintUnsignedInteger((UINT64)-val, base, false, buf + 1, size - 1) + 1;
	}
	return printf_PrintUnsignedInteger((UINT64)val, base, false, buf, size);
}

static USIZE printf_PrintString(const char *str, char *buf, USIZE size) {
	USIZE pos = 0;
	for (UINT64 i = 0; str[i] != '\0'; ++i) {
		if (pos >= size) {
			return pos;
		}
		buf[pos] = str[i];
		++pos;
	}
	return pos;
}

static USIZE printf_PrintPointer(UINTN pointer, int depth, char *buf, USIZE size) {
	if (depth == 0) {
		return 0;
	}
	if (size == 0) {
		return 0;
	}
	USIZE used = printf_PrintPointer(pointer / 16, depth - 1, buf, size);
	if (size == used) {
		return used;
	}
	buf[used] = printf_GetCharFromDigit(pointer % 16);
	return used + 1;
}

USIZE printf(const char *fmt, ...) {
	va_list args;
	va_start(args, fmt);
	USIZE result = va_printf(fmt, args);
	va_end(args);
	return result;
}

USIZE sprintf(const char *fmt, char *buf, USIZE size, ...) {
	va_list args;
	va_start(args, size);
	USIZE result = va_sprintf(fmt, buf, size, args);
	va_end(args);
	return result;
}

USIZE va_sprintf(const char *fmt, char *buf, USIZE size, va_list args) {
	USIZE pos = 0;
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
				pos += printf_PrintInteger((INT64)va_arg(args, INT32), 10, buf + pos, size - pos);
				break;
			case 'u':
				pos += printf_PrintUnsignedInteger((UINT64)va_arg(args, UINT32), 10, false, buf + pos, size - pos);
				break;
			case 'p':
				pos += printf_PrintPointer((UINT64)va_arg(args, UINTN), sizeof(UINTN) * 2, buf + pos, size - pos);
				break;
			case 's':
				pos += printf_PrintString(va_arg(args, char *), buf + pos, size - pos);
				break;
			case 'c':
				buf[pos] = (char)va_arg(args, int);
				++pos;
				break;
			case 'l':
				++i;
				switch (fmt[i]) {
				case 'u':
					pos += printf_PrintUnsignedInteger(va_arg(args, UINT64), 10, false, buf + pos, size - pos);
					break;
				case 'd':
					pos += printf_PrintInteger(va_arg(args, INT64), 10, buf + pos, size - pos);
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

void printf_WriteStrinig(const char *str, UINT64 size) {
	for (UINT64 i = 0; i < size; ++i) {
		HAL_TTY_PrintCharacter(str[i]);
	}
	HAL_TTY_Flush();
}

USIZE va_printf(const char *str, va_list args) {
	char buf[1024];
	USIZE count = va_sprintf(str, buf, 1024, args);
	printf_WriteStrinig(buf, count);
	return count;
}
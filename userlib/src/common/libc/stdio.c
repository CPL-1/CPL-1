#include <errno.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/libsyscall.h>

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

int sprintf(const char *fmt, char *buf, int size, ...) {
	va_list args;
	va_start(args, size);
	int result = va_sprintf(fmt, buf, size, args);
	va_end(args);
	return result;
}

int va_sprintf(const char *fmt, char *buf, int size, va_list args) {
	int pos = 0;
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
	int count = va_sprintf(str, buf, PRINTF_BUFFER_SIZE, args);
	__Printf_WriteString(buf, count);
	return count;
}

struct __FILE {
	long offset;
	int fd;
};

#define FILE struct __FILE

FILE *fopen(const char *name, const char *attr) {
	int perm = 0;
	if (strcmp(attr, "r") == 0 || strcmp(attr, "rb") == 0) {
		perm = O_RDONLY;
	} else if (strcmp(attr, "w") == 0 || strcmp(attr, "wb") == 0) {
		perm = O_WRONLY;
	} else if (strcmp(attr, "r+") == 0 || strcmp(attr, "rb+") == 0 || strcmp(attr, "r+b") == 0) {
		perm = O_RDWR;
	} else {
		errno = EINVAL;
		return NULL;
	}
	FILE *f = malloc(sizeof(FILE));
	if (f == NULL) {
		return NULL;
	}
	int fd = open(name, perm);
	if (fd == -1) {
		free(f);
		return NULL;
	}
	f->fd = fd;
	f->offset = 0;
	return f;
}

static size_t __File_Read(int fd, void *buf, size_t size) {
	size_t pos = 0;
	while (size != 0) {
		int result = read(fd, buf + pos, size);
		if (result < 0) {
			errno = result;
			return 0;
		}
		if (result == 0) {
			return pos;
		}
		pos += (size_t)result;
	}
	return pos;
}

static size_t __File_Write(int fd, const void *buf, size_t size) {
	size_t pos = 0;
	while (size != 0) {
		int result = write(fd, buf + pos, size);
		if (result < 0) {
			errno = result;
			return 0;
		}
		if (result == 0) {
			return pos;
		}
		pos += (size_t)result;
	}
	return pos;
}

size_t fread(void *buf, size_t size, size_t nmemb, FILE *stream) {
	int result = __File_Read(stream->fd, buf, size * nmemb);
	if (result < 0) {
		errno = result;
		return 0;
	}
	return result / ((int)size);
}

size_t fwrite(const void *buf, size_t size, size_t nmemb, FILE *stream) {
	int result = __File_Write(stream->fd, buf, size * nmemb);
	if (result < 0) {
		errno = result;
		return 0;
	}
	return result / ((int)size);
}

int fclose(FILE *stream) {
	int result = close(stream->fd);
	free(stream);
	return result;
}
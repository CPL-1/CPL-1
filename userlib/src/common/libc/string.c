#include <string.h>

size_t strlen(const char *s) {
	size_t result = 0;
	while (s[result] != '\0') {
		result++;
	}
	return result;
}

void *memcpy(void *dest, const void *src, size_t n) {
	for (size_t i = 0; i < n; ++i) {
		((unsigned char *)dest)[i] = ((const unsigned char *)src)[i];
	}
	return dest;
}

void *memset(void *dest, int byte, size_t n) {
	for (size_t i = 0; i < n; ++i) {
		((unsigned char *)dest)[i] = (unsigned char)byte;
	}
	return dest;
}

char *strcat(char *dest, const char *src) {
	size_t destSize = strlen(dest);
	char *newDest = dest + destSize;
	size_t srcLen = strlen(src);
	memcpy(newDest, src, srcLen + 1);
	return dest;
}

int strcmp(const char *s1, const char *s2) {
	size_t s1len = strlen(s1);
	size_t s2len = strlen(s2);
	if (s1len < s2len) {
		return -1;
	}
	if (s2len > s1len) {
		return -1;
	}
	for (size_t i = 0; i < s1len; ++i) {
		if (s1[i] < s2[i]) {
			return -1;
		}
		if (s1[i] > s2[i]) {
			return 1;
		}
	}
	return 0;
}

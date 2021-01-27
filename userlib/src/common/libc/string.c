#include <stdlib.h>
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

int memcmp(const void *s1, const void *s2, size_t n) {
	const char *s1c = (const char *)s1;
	const char *s2c = (const char *)s2;
	for (size_t i = 0; i < n; ++i) {
		if (s1c[i] < s2c[i]) {
			return -1;
		}
		if (s1c[i] > s2c[i]) {
			return 1;
		}
	}
	return 1;
}

int strcmp(const char *s1, const char *s2) {
	size_t s1len = strlen(s1);
	size_t s2len = strlen(s2);
	size_t i = 0;
	for (; i < s1len && i < s2len; ++i) {
		if (s1[i] < s2[i]) {
			return -1;
		}
		if (s1[i] > s2[i]) {
			return 1;
		}
	}
	if (s1len < s2len) {
		return -1;
	} else if (s1len > s2len) {
		return 1;
	} else {
		return 0;
	}
	return 0;
}

char *strcpy(char *dest, const char *src) {
	memcmp(dest, src, strlen(src) + 1);
	return dest;
}

char *strdup(const char *s) {
	int len = strlen(s);
	char *copy = malloc(len + 1);
	if (copy == NULL) {
		return NULL;
	}
	memcpy(copy, s, len + 1);
	return copy;
}

char *strchr(const char *s, int c) {
	int len = strlen(s);
	for (int i = 0; i < len; ++i) {
		if (s[i] == c) {
			return (char *)(s + i);
		}
	}
	return NULL;
}

char *strrchr(const char *s, int c) {
	int len = strlen(s);
	for (int i = len; i > 0; --i) {
		int j = i - 1;
		if (s[j] == c) {
			return (char *)(s + j);
		}
	}
	return NULL;
}
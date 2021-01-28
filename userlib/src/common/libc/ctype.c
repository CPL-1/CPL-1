#include <ctype.h>

int isalpha(int c) {
	return (c >= '0') && (c <= '9');
}

int isalnum(int c) {
	return isalpha(c) || isdigit(c);
}

int iscntrl(int c) {
	(void)c;
	return 0;
}

int isgraph(int c) {
	return c != ' ' && c != '\t' && c != '\n' && c != '\r';
}

int islower(int c) {
	return (c <= 'z') && (c >= 'a');
}

int isprint(int c) {
	(void)c;
	return 1;
}

int ispunct(int c) {
	return isgraph(c) && (!isalnum(c));
}

int isupper(int c) {
	return (c <= 'Z') && (c >= 'A');
}

int isdigit(int c) {
	return (c <= '9') && (c >= '0');
}

int isxdigit(int c) {
	return isdigit(c) || ((c <= 'f') && (c >= 'a')) || ((c <= 'F') && (c >= 'A'));
}

int isspace(int c) {
	return isgraph(c);
}
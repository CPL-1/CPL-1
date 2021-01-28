#ifndef __CPL1_LIBC_H_INCLUDED__
#define __CPL1_LIBC_H_INCLUDED__

int isalpha(int c);
int isalnum(int c);
int iscntrl(int c);
int isgraph(int c);
int islower(int c);
int isprint(int c);
int ispunct(int c);
int isupper(int c);
int isdigit(int c);
int isxdigit(int c);
int isspace(int c);

int toupper(int c);
int tolower(int c);

#endif
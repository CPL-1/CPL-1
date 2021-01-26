#ifndef __READLINE_H_INCLUDED__
#define __READLINE_H_INCLUDED__

#include <common/misc/utils.h>

void ReadLine_Initialize();
size_t ReadLine(char *buf, size_t size);

#endif
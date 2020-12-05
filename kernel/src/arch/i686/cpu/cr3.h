#ifndef __I686_CR3_H_INCLUDED__
#define __I686_CR3_H_INCLUDED__

#include <common/misc/utils.h>

void i686_CR3_Initialize();
void i686_CR3_Set(UINT32 val);
UINT32 i686_CR3_Get();

#endif
#ifndef __I686_TSS_H_INCLUDED__
#define __I686_TSS_H_INCLUDED__

#include <common/misc/utils.h>

void i686_TSS_Initialize();
UINT32 i686_TSS_GetBase();
UINT32 i686_TSS_GetLimit();
void i686_TSS_SetISRStack(UINT32 esp, UINT16 ss);
void i686_TSS_SetKernelStack(UINT32 esp, UINT16 ss);

#endif
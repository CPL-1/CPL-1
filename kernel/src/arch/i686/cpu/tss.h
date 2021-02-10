#ifndef __I686_TSS_H_INCLUDED__
#define __I686_TSS_H_INCLUDED__

#include <common/misc/utils.h>

void i686_TSS_Initialize();
uint32_t i686_TSS_GetBase();
uint32_t i686_TSS_GetLimit();
void i686_TSS_SetISRStack(uint32_t esp, uint16_t ss);
void i686_TSS_SetKernelStack(uint32_t esp, uint16_t ss);

#endif

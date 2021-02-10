#ifndef __I686_RING3_H_INCLUDED__
#define __I686_RING3_H_INCLUDED__

#include <common/misc/utils.h>

void i686_Ring3_SyscallInit();
void i686_Ring3_Switch(uint32_t entryPoint, uint32_t stack);

#endif

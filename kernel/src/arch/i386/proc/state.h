#ifndef __I386_STATE_H_INCLUDED__
#define __I386_STATE_H_INCLUDED__

#include <common/misc/utils.h>

struct i386_cpu_state {
	uint32_t ds, gs, fs, es;
	uint32_t edi, esi, ebp;
	uint32_t : 32;
	uint32_t ebx, edx, ecx, eax;
	uint32_t eip, cs, eflags, esp, ss;
} packed;

#endif

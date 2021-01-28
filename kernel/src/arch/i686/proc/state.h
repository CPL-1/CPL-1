#ifndef __I686_STATE_H_INCLUDED__
#define __I686_STATE_H_INCLUDED__

#include <common/misc/utils.h>

struct i686_CPUState {
	uint32_t gs, fs, ds, es;
	uint32_t edi, esi, ebp;
	uint32_t : 32;
	uint32_t ebx, edx, ecx, eax;
	uint32_t errorcode, eip, cs, eflags, esp, ss;
} PACKED;

#endif

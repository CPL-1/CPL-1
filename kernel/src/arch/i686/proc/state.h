#ifndef __I686_STATE_H_INCLUDED__
#define __I686_STATE_H_INCLUDED__

#include <common/misc/utils.h>

struct i686_cpu_state {
	UINT32 ds, gs, fs, es;
	UINT32 edi, esi, ebp;
	UINT32 : 32;
	UINT32 ebx, edx, ecx, eax;
	UINT32 eip, cs, eflags, esp, ss;
} PACKED;

#endif

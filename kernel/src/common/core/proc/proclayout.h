#ifndef __PROCLAYOUT_H_INCLUDED__
#define __PROCLAYOUT_H_INCLUDED__

#include <common/core/proc/proc.h>

#define PROC_KERNEL_STACK_SIZE 4096

struct Proc_Process {
	struct Proc_ProcessID pid, ppid;
	struct Proc_Process *next, *prev;
	struct Proc_Process *waitQueueHead;
	struct Proc_Process *waitQueueTail;
	struct Proc_Process *nextInQueue;
	char *processState;
	struct VirtualMM_AddressSpace *addressSpace;
	uintptr_t kernelStack;
	int returnCode;
	enum
	{
		SLEEPING,
		RUNNING,
		WAITING_FOR_CHILD_TERM,
		ZOMBIE
	} state;
};

#endif

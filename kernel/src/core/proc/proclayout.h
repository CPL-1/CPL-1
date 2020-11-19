#ifndef __PROCLAYOUT_H_INCLUDED__
#define __PROCLAYOUT_H_INCLUDED__

#include <core/proc/proc.h>

#define PROC_KERNEL_STACK_SIZE 4096

struct proc_trap_frame {
	uint32_t ds, gs, fs, es;
	uint32_t edi, esi, ebp;
	uint32_t : 32;
	uint32_t ebx, edx, ecx, eax;
	uint32_t eip, cs, eflags, esp, ss;
} packed;

struct proc_process {
	struct proc_trap_frame frame;
	struct proc_id pid, ppid;
	struct proc_process *next, *prev;
	struct proc_process *wait_queue_head;
	struct proc_process *wait_queue_tail;
	struct proc_process *next_in_queue;
	uint32_t cr3;
	uint32_t kernel_stack;
	int return_code;
	enum { SLEEPING, RUNNING, WAITING_FOR_CHILD_TERM, ZOMBIE } state;
};

#endif

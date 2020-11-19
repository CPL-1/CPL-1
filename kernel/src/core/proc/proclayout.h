#ifndef __PROCLAYOUT_H_INCLUDED__
#define __PROCLAYOUT_H_INCLUDED__

#include <core/proc/proc.h>

#define PROC_KERNEL_STACK_SIZE 4096

struct proc_process {
	struct proc_id pid, ppid;
	struct proc_process *next, *prev;
	struct proc_process *wait_queue_head;
	struct proc_process *wait_queue_tail;
	struct proc_process *next_in_queue;
	char *process_state;
	uintptr_t virt_root;
	uintptr_t kernel_stack;
	int return_code;
	enum { SLEEPING, RUNNING, WAITING_FOR_CHILD_TERM, ZOMBIE } state;
};

#endif

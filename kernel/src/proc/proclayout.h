#ifndef __PROCLAYOUT_H_INCLUDED__
#define __PROCLAYOUT_H_INCLUDED__

struct proc_trap_frame {
	uint32_t ds, gs, fs, es;
	uint32_t edi, esi, ebp;
	uint32_t : 32;
	uint32_t ebx, edx, ecx, eax;
	uint32_t eip, cs, eflags, esp, ss;
} packed;

#define PROC_MAX_NAME_LENGTH 16

struct proc_thread {
	struct proc_thread *next, *prev;
	struct proc_process *process;
	struct proc_trap_frame frame;
	size_t thread_id;
	struct {
		uint32_t bottom;
		uint32_t top;
	} kernel_stack;
	bool is_sleeping;
} packed;

struct proc_process {
	struct proc_thread *head;
	struct proc_process *next, *prev;
	size_t sleeping_threads_count;
	size_t awake_threads_count;
	size_t process_id;
	bool preempt_threads;

	uint32_t cr3;
	char name[PROC_MAX_NAME_LENGTH];
} packed;

#endif
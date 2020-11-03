#include <drivers/pit.h>
#include <i386/tss.h>
#include <kmsg.h>
#include <memory/heap.h>
#include <memory/virt.h>
#include <proc/intlock.h>
#include <proc/proc.h>
#include <proc/proclayout.h>

#define MAX_PROCESS_ID 4096
#define MAX_THREAD_ID 16384

static struct proc_thread *proc_threads_by_id[MAX_THREAD_ID];
static struct proc_process *proc_processes_by_id[MAX_PROCESS_ID];
static struct proc_process *proc_current_process;

#define SCHEDULER_STACK_SIZE 4096
static char proc_stack[SCHEDULER_STACK_SIZE];

#define KERNEL_STACK_SIZE 4096

static size_t proc_allocate_pid() {
	for (size_t i = 0; i < MAX_PROCESS_ID; ++i) {
		if (proc_processes_by_id[i] == NULL) {
			return i;
		}
	}
	return MAX_PROCESS_ID;
}

static size_t proc_allocate_tid() {
	for (size_t i = 0; i < MAX_THREAD_ID; ++i) {
		if (proc_threads_by_id[i] == NULL) {
			return i;
		}
	}
	return MAX_THREAD_ID;
}

struct proc_process *proc_new_process(const char *name) {
	size_t name_length = strlen(name);
	if (name_length >= PROC_MAX_NAME_LENGTH) {
		return NULL;
	}
	size_t id = proc_allocate_pid();
	if (id == MAX_PROCESS_ID) {
		return NULL;
	}
	struct proc_process *pointer = ALLOC_OBJ(struct proc_process);
	if (pointer == NULL) {
		return NULL;
	}
	proc_processes_by_id[id] = pointer;
	pointer->next = pointer->prev = NULL;
	pointer->head = NULL;
	pointer->process_id = id;
	pointer->cr3 = 0;
	pointer->sleeping_threads_count = pointer->awake_threads_count = 0;
	memset(&(pointer->name), 0, sizeof(pointer->name));
	memcpy(&(pointer->name), name, name_length);
	return pointer;
}

struct proc_thread *proc_new_thread() {
	size_t id = proc_allocate_tid();
	if (id == MAX_THREAD_ID) {
		return NULL;
	}
	struct proc_thread *pointer = ALLOC_OBJ(struct proc_thread);
	if (pointer == NULL) {
		return NULL;
	}
	pointer->next = pointer->prev = NULL;
	pointer->thread_id = id;
	memset(&(pointer->frame), 0, sizeof(pointer->frame));
	return pointer;
}

struct proc_process *proc_get_process_by_id(size_t id) {
	if (id > MAX_PROCESS_ID) {
		return NULL;
	}
	return proc_processes_by_id[id];
}

struct proc_thread *proc_get_thread_by_id(size_t id) {
	if (id > MAX_THREAD_ID) {
		return NULL;
	}
	return proc_threads_by_id[id];
}

void proc_wake_process(struct proc_process *proc) {
	intlock_lock();
	if (proc_current_process == NULL) {
		proc_current_process = proc;
		proc->next = proc;
		proc->prev = proc;
	} else {
		struct proc_process *next = proc_current_process;
		struct proc_process *prev = proc_current_process->prev;
		next->prev = proc;
		prev->next = proc;
		proc->next = next;
		proc->prev = prev;
	}
	intlock_unlock();
}

void proc_thread_attach(struct proc_process *proc, struct proc_thread *thread) {
	intlock_lock();
	thread->process = proc;
	if (proc->head == NULL) {
		// No one is awake => process is sleeping
		// Let's add it to the scheduler queue
		proc->head = thread;
		thread->next = thread;
		thread->prev = thread;
	} else {
		// Process is already in the active queue, no need to wake up
		struct proc_thread *head = proc->head;
		struct proc_thread *prev = head->prev;
		head->prev = thread;
		prev->next = thread;
		thread->next = head;
		thread->prev = prev;
	}
	if (thread->is_sleeping) {
		// Old thread just recovered from the sleep
		if (proc->sleeping_threads_count) {
			// This is also an error
			// Thread can't sleep if sleeping_threads_count is 0
			kmsg_err("Scheduler",
			         "sleeping_threads_count == 0, but somehow thread tried "
			         "to wake up");
		}
		proc->sleeping_threads_count--;
		proc->awake_threads_count++;
	} else {
		// Completely new thread is attached
		proc->awake_threads_count++;
	}
	intlock_unlock();
}

static void proc_schedule_handler(struct proc_trap_frame *frame) {
	// TODO: what if the only thread in current process died
	// TODO: what if the thread went to sleep
	// TODO: update kernel stacks
	// TODO: update floating point context

	// Copying trap frame to the thread control block
	struct proc_thread *current_thread = proc_current_process->head;
	memcpy(&(current_thread->frame), frame, sizeof(*frame));
	// yield to the next thread
	proc_current_process->head = current_thread->next;
	// move to the next process and next thread
	proc_current_process = proc_current_process->next;
	current_thread = proc_current_process->head;
	// copy its values
	memcpy(frame, &(current_thread->frame), sizeof(*frame));
	// we are good to go
}

void proc_init() {
	// Initializing everything with zeroes
	proc_current_process = NULL;
	memset(&proc_threads_by_id, 0, sizeof(proc_threads_by_id));
	memset(&proc_processes_by_id, 0, sizeof(proc_threads_by_id));
	// Allocating kernel process and init kernel thread
	struct proc_process *kernel_process = proc_new_process("Kernel");
	struct proc_thread *kernel_init_thread = proc_new_thread();
	// Attaching init kernel thread to the kernel process
	proc_thread_attach(kernel_process, kernel_init_thread);
	// "Waking up" kernel process
	proc_wake_process(kernel_process);
	tss_set_dpl0_stack((uint32_t)(proc_stack + SCHEDULER_STACK_SIZE), 0x10);
	pit_set_callback((uint32_t)proc_schedule_handler);
}

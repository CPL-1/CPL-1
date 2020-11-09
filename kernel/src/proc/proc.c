#include <drivers/pit.h>
#include <i386/cpu.h>
#include <i386/tss.h>
#include <kmsg.h>
#include <memory/heap.h>
#include <memory/phys.h>
#include <memory/virt.h>
#include <proc/intlock.h>
#include <proc/proc.h>
#include <proc/proclayout.h>

#define PROC_MOD_NAME "Process Manager & Scheduler"

static uint64_t proc_instance_count_by_id[MAX_PROC_COUNT];
static struct proc_process *proc_processes_by_id[MAX_PROC_COUNT];
static struct proc_process *proc_current_process;
static struct proc_process *proc_dealloc_queue_head;
static bool proc_initialized = false;

#define PROC_SCHEDULER_STACK_SIZE 4096
static char proc_scheduler_stack[PROC_SCHEDULER_STACK_SIZE];

bool proc_is_initialized() { return proc_initialized; }

struct proc_process *proc_get_data(struct proc_id id) {
	size_t array_index = id.id;
	if (array_index >= MAX_PROC_COUNT) {
		return NULL;
	}
	intlock_lock();
	struct proc_process *data = proc_processes_by_id[array_index];
	if (data == NULL) {
		intlock_unlock();
		return NULL;
	}
	if (proc_instance_count_by_id[array_index] != id.instance_number) {
		intlock_unlock();
		return NULL;
	}
	intlock_unlock();
	return data;
}

static struct proc_id proc_allocate_proc_id(struct proc_process *process) {
	struct proc_id result;
	for (size_t i = 0; i < MAX_PROC_COUNT; ++i) {
		intlock_lock();
		if (proc_processes_by_id[i] == NULL) {
			proc_processes_by_id[i] = process;
			result.id = i;
			result.instance_number = proc_instance_count_by_id[i];
			intlock_unlock();
			return result;
		}
		intlock_unlock();
		asm volatile("pause");
	}
	result.id = MAX_PROC_COUNT;
	result.instance_number = 0;
	return result;
}

struct proc_id proc_new_process(struct proc_id parent) {
	struct proc_process *process = ALLOC_OBJ(struct proc_process);
	if (process == NULL) {
		goto fail;
	}
	uint32_t new_cr3 = virt_new_cr3();
	if (new_cr3 == 0) {
		goto free_process_obj;
	}
	uint32_t stack = (uint32_t)heap_alloc(PROC_KERNEL_STACK_SIZE);
	if (stack == 0) {
		goto free_cr3;
	}
	struct proc_id new_id = proc_allocate_proc_id(process);
	if (!proc_is_valid_proc_id(new_id)) {
		goto free_stack;
	}
	memset(&(process->frame), 0, sizeof(process->frame));
	process->next = process->prev = process->wait_queue_head =
	    process->wait_queue_tail = process->next_in_queue = NULL;
	process->ppid = parent;
	process->pid = new_id;
	process->cr3 = new_cr3;
	process->kernel_stack = stack;
	process->return_code = 0;
	process->state = SLEEPING;
	return new_id;
free_stack:
	heap_free((void *)stack, PROC_KERNEL_STACK_SIZE);
free_cr3:
	phys_free_frame(new_cr3);
free_process_obj:
	FREE_OBJ(process);
fail:;
	struct proc_id failed_id;
	failed_id.instance_number = 0;
	failed_id.id = MAX_PROC_COUNT;
	return failed_id;
}

void proc_continue(struct proc_id id) {
	struct proc_process *process = proc_get_data(id);
	if (process == NULL) {
		return;
	}
	intlock_lock();
	process->state = RUNNING;
	struct proc_process *next, *prev;
	next = proc_current_process;
	prev = proc_current_process->prev;
	next->prev = process;
	prev->next = process;
	process->next = next;
	process->prev = prev;
	intlock_unlock();
}

static void proc_cut_from_the_list(struct proc_process *process) {
	struct proc_process *prev = process->prev;
	struct proc_process *next = process->next;
	prev->next = next;
	next->prev = prev;
}

void proc_pause(struct proc_id id, bool override_state) {
	struct proc_process *process = proc_get_data(id);
	if (process == NULL) {
		return;
	}
	intlock_lock();
	if (override_state) {
		process->state = SLEEPING;
	}
	proc_cut_from_the_list(process);
	if (process == proc_current_process) {
		proc_yield();
	}
	intlock_unlock();
}

struct proc_id proc_my_id() {
	struct proc_process *current = proc_current_process;
	struct proc_id id = current->pid;
	return id;
}

void proc_pause_self(bool override_state) {
	proc_pause(proc_my_id(), override_state);
}

void proc_dispose(struct proc_process *process) {
	intlock_lock();
	process->next_in_queue = proc_dealloc_queue_head;
	proc_dealloc_queue_head = process;
	intlock_unlock();
}

void proc_exit(int exit_code) {
	intlock_lock();
	struct proc_process *process = proc_current_process;
	process->return_code = exit_code;
	proc_instance_count_by_id[process->pid.id]++;
	proc_processes_by_id[process->pid.id] = NULL;
	process->state = ZOMBIE;
	proc_cut_from_the_list(process);
	struct proc_id parent_id = process->ppid;
	struct proc_process *parent_process = proc_get_data(parent_id);
	if (parent_process == NULL) {
		proc_dispose(process);
	} else {
		if (parent_process->wait_queue_head == NULL) {
			parent_process->wait_queue_head = parent_process->wait_queue_tail =
			    process;
		} else {
			parent_process->wait_queue_tail->next_in_queue = process;
		}
		process->next_in_queue = NULL;
		if (parent_process->state == WAITING_FOR_CHILD_TERM) {
			proc_continue(parent_id);
		}
	}
	proc_yield();
}

struct proc_process *proc_get_waiting_queue_head(struct proc_process *process) {
	struct proc_process *result = process->wait_queue_head;
	if (process->wait_queue_tail == result) {
		process->wait_queue_head = process->wait_queue_tail = NULL;
	} else {
		process->wait_queue_head = process->wait_queue_head->next_in_queue;
	}
	return result;
}

struct proc_process *proc_wait_for_child_term() {
	intlock_lock();
	struct proc_process *process = proc_current_process;
	if (process->wait_queue_head != NULL) {
		struct proc_process *result = proc_get_waiting_queue_head(process);
		intlock_unlock();
		return result;
	}
	process->state = WAITING_FOR_CHILD_TERM;
	proc_pause_self(false);
	intlock_lock();
	struct proc_process *result = proc_get_waiting_queue_head(process);
	intlock_unlock();
	return result;
}

void proc_yield() {
	intlock_flush();
	pit_trigger_interrupt();
}

void proc_preempt(unused void *ctx, struct proc_trap_frame *frame) {
	memcpy(&(proc_current_process->frame), frame,
	       sizeof(struct proc_trap_frame));
	proc_current_process = proc_current_process->next;
	memcpy(frame, &(proc_current_process->frame),
	       sizeof(struct proc_trap_frame));
	cpu_set_cr3(proc_current_process->cr3);
	tss_set_dpl1_stack(
	    proc_current_process->kernel_stack + PROC_KERNEL_STACK_SIZE, 0x21);
}

void proc_init() {
	for (size_t i = 0; i < MAX_PROC_COUNT; ++i) {
		proc_processes_by_id[i] = NULL;
		proc_instance_count_by_id[i] = 0;
	}
	struct proc_id fake_proc_id;
	fake_proc_id.id = 0;
	fake_proc_id.instance_number = 0;
	struct proc_id kernel_proc_id = proc_new_process(fake_proc_id);
	if (!proc_is_valid_proc_id(kernel_proc_id)) {
		kmsg_err(PROC_MOD_NAME, "Failed to allocate kernel process");
	}
	struct proc_process *kernel_process_data = proc_get_data(kernel_proc_id);
	if (kernel_process_data == NULL) {
		kmsg_err(PROC_MOD_NAME, "Failed to access data of the kernel process");
	}
	kernel_process_data->state = RUNNING;
	proc_current_process = kernel_process_data;
	kernel_process_data->next = kernel_process_data;
	kernel_process_data->prev = kernel_process_data;
	proc_dealloc_queue_head = NULL;
	tss_set_dpl0_stack(
	    (uint32_t)proc_scheduler_stack + PROC_SCHEDULER_STACK_SIZE, 0x10);
	pit_set_callback((uint32_t)proc_preempt);
	proc_initialized = true;
}

bool proc_dispose_queue_poll() {
	intlock_lock();
	struct proc_process *process = proc_dealloc_queue_head;
	if (process == NULL) {
		intlock_unlock();
		return false;
	}
	kmsg_log("User Request Monitor", "Disposing process...");
	proc_dealloc_queue_head = process->next_in_queue;
	intlock_unlock();
	if (process->cr3 != 0) {
		phys_free_frame(process->cr3);
	}
	if (process->kernel_stack != 0) {
		heap_free((void *)(process->kernel_stack), PROC_KERNEL_STACK_SIZE);
	}
	FREE_OBJ(process);
	return true;
}
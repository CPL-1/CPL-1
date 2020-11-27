#include <common/core/memory/heap.h>
#include <common/core/memory/virt.h>
#include <common/core/proc/proc.h>
#include <common/core/proc/proclayout.h>
#include <common/lib/kmsg.h>
#include <hal/memory/phys.h>
#include <hal/memory/virt.h>
#include <hal/proc/intlock.h>
#include <hal/proc/isrhandler.h>
#include <hal/proc/stack.h>
#include <hal/proc/state.h>
#include <hal/proc/timer.h>

#define PROC_MOD_NAME "Process Manager & Scheduler"

static uint64_t proc_instance_count_by_id[MAX_PROC_COUNT];
static struct proc_process *proc_processes_by_id[MAX_PROC_COUNT];
static struct proc_process *proc_current_process;
static struct proc_process *proc_dealloc_queue_head;
static bool proc_initialized = false;

#define PROC_SCHEDULER_STACK_SIZE 65536
static char proc_scheduler_stack[PROC_SCHEDULER_STACK_SIZE];

bool proc_is_initialized() { return proc_initialized; }

struct proc_process *proc_get_data(struct proc_id id) {
	size_t array_index = id.id;
	if (array_index >= MAX_PROC_COUNT) {
		return NULL;
	}
	hal_intlock_lock();
	struct proc_process *data = proc_processes_by_id[array_index];
	if (data == NULL) {
		hal_intlock_unlock();
		return NULL;
	}
	if (proc_instance_count_by_id[array_index] != id.instance_number) {
		hal_intlock_unlock();
		return NULL;
	}
	hal_intlock_unlock();
	return data;
}

static struct proc_id proc_allocate_proc_id(struct proc_process *process) {
	struct proc_id result;
	for (size_t i = 0; i < MAX_PROC_COUNT; ++i) {
		hal_intlock_lock();
		if (proc_processes_by_id[i] == NULL) {
			proc_processes_by_id[i] = process;
			result.id = i;
			result.instance_number = proc_instance_count_by_id[i];
			hal_intlock_unlock();
			return result;
		}
		hal_intlock_unlock();
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
	uintptr_t stack = (uintptr_t)heap_alloc(PROC_KERNEL_STACK_SIZE);
	if (stack == 0) {
		goto free_process_obj;
	}
	char *process_state = (char *)(heap_alloc(HAL_PROCESS_STATE_SIZE));
	if (process_state == NULL) {
		goto free_stack;
	}
	struct proc_id new_id = proc_allocate_proc_id(process);
	if (!proc_is_valid_proc_id(new_id)) {
		goto free_process_state;
	}
	memset(process_state, 0, HAL_PROCESS_STATE_SIZE);
	process->next = process->prev = process->wait_queue_head =
		process->wait_queue_tail = process->next_in_queue = NULL;
	process->ppid = parent;
	process->pid = new_id;
	process->process_state = process_state;
	process->kernel_stack = stack;
	process->return_code = 0;
	process->state = SLEEPING;
	process->address_space = NULL;
	return new_id;
free_process_state:
	heap_free(process_state, HAL_PROCESS_STATE_SIZE);
free_stack:
	heap_free((void *)stack, PROC_KERNEL_STACK_SIZE);
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
	hal_intlock_lock();
	process->state = RUNNING;
	struct proc_process *next, *prev;
	next = proc_current_process;
	prev = proc_current_process->prev;
	next->prev = process;
	prev->next = process;
	process->next = next;
	process->prev = prev;
	hal_intlock_unlock();
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
	hal_intlock_lock();
	if (override_state) {
		process->state = SLEEPING;
	}
	proc_cut_from_the_list(process);
	if (process == proc_current_process) {
		proc_yield();
	}
	hal_intlock_unlock();
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
	hal_intlock_lock();
	process->next_in_queue = proc_dealloc_queue_head;
	proc_dealloc_queue_head = process;
	hal_intlock_unlock();
}

void proc_exit(int exit_code) {
	hal_intlock_lock();
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
	hal_intlock_lock();
	struct proc_process *process = proc_current_process;
	if (process->wait_queue_head != NULL) {
		struct proc_process *result = proc_get_waiting_queue_head(process);
		hal_intlock_unlock();
		return result;
	}
	process->state = WAITING_FOR_CHILD_TERM;
	proc_pause_self(false);
	hal_intlock_lock();
	struct proc_process *result = proc_get_waiting_queue_head(process);
	hal_intlock_unlock();
	return result;
}

void proc_yield() {
	hal_intlock_flush();
	hal_timer_trigger_callback();
}

void proc_preempt(unused void *ctx, char *frame) {
	memcpy(proc_current_process->process_state, frame, HAL_PROCESS_STATE_SIZE);
	proc_current_process = proc_current_process->next;
	memcpy(frame, proc_current_process->process_state, HAL_PROCESS_STATE_SIZE);
	virt_switch_to_address_space(proc_current_process->address_space);
	hal_stack_syscall_set(proc_current_process->kernel_stack +
						  PROC_KERNEL_STACK_SIZE);
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
	kernel_process_data->process_state = heap_alloc(HAL_PROCESS_STATE_SIZE);
	if (kernel_process_data->process_state == NULL) {
		kmsg_err(PROC_MOD_NAME, "Failed to allocate kernel process state");
	}
	kernel_process_data->address_space =
		virt_make_address_space_from_root(hal_virt_get_root());
	if (kernel_process_data->address_space == NULL) {
		kmsg_err(PROC_MOD_NAME,
				 "Failed to allocate process address space object");
	}
	proc_dealloc_queue_head = NULL;
	hal_stack_isr_set((uintptr_t)(proc_scheduler_stack) +
					  PROC_SCHEDULER_STACK_SIZE);
	if (!hal_timer_set_callback((hal_isr_handler_t)proc_preempt)) {
		kmsg_err(PROC_MOD_NAME, "Failed to set timer callback");
	}
	proc_initialized = true;
}

bool proc_dispose_queue_poll() {
	hal_intlock_lock();
	struct proc_process *process = proc_dealloc_queue_head;
	if (process == NULL) {
		hal_intlock_unlock();
		return false;
	}
	kmsg_log("User Request Monitor", "Disposing process...");
	proc_dealloc_queue_head = process->next_in_queue;
	hal_intlock_unlock();
	if (process->address_space != 0) {
		virt_drop_address_space(process->address_space);
	}
	if (process->kernel_stack != 0) {
		heap_free((void *)(process->kernel_stack), PROC_KERNEL_STACK_SIZE);
	}
	FREE_OBJ(process);
	return true;
}
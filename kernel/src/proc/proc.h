#ifndef __PROC_H_INCLUDED__
#define __PROC_H_INCLUDED__

#define MAX_PROC_COUNT 4096
#define PROC_KERNEL_STACK_SIZE 4096
#define INVALID_PROC_ID                                                        \
	(struct proc_id) { .id = MAX_PROC_COUNT, .instance_number = 0 }

struct proc_id {
	uint64_t id;
	uint64_t instance_number;
};

static inline bool proc_is_valid_proc_id(struct proc_id id) {
	return id.id != MAX_PROC_COUNT;
}

void proc_init();
bool proc_is_initialized();

struct proc_id proc_new_process(struct proc_id parent);

void proc_yield();
struct proc_id proc_my_id();
void proc_pause_self(bool override_state);
void proc_pause(struct proc_id id, bool override_state);
void proc_continue(struct proc_id id);
struct proc_process *proc_wait_for_child_term();
struct proc_process *proc_get_data(struct proc_id id);

void proc_exit(int exit_code);
void proc_dispose(struct proc_process *process);

void proc_dispose_queue_poll();

#endif

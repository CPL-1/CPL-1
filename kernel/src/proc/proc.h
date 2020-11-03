#ifndef __PROC_H_INCLUDED__
#define __PROC_H_INCLUDED__

#include <utils.h>

void proc_init();
struct proc_process *proc_new_process(const char *name);
struct proc_thread *proc_new_thread();

struct proc_process *proc_get_process_by_id(size_t id);
struct proc_thread *proc_get_thread_by_id(size_t id);

void proc_wake_process(struct proc_process *proc);
void proc_thread_attach(struct proc_process *proc, struct proc_thread *thread);

bool proc_start_new_kernel_thread(uint32_t entrypoint);
#endif

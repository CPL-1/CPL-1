#include <core/proc/intlock.h>
#include <core/proc/mutex.h>
#include <core/proc/proc.h>
#include <core/proc/proclayout.h>
#include <lib/kmsg.h>

void mutex_init(struct mutex *mutex) {
	mutex->queue_head = mutex->queue_tail = NULL;
	mutex->locked = false;
}

void mutex_lock(struct mutex *mutex) {
	if (!proc_is_initialized()) {
		return;
	}
	intlock_lock();
	struct proc_process *process = proc_get_data(proc_my_id());
	if (process == NULL) {
		kmsg_err("Mutex Manager", "Failed to get current process data");
	}
	if (!(mutex->locked)) {
		mutex->locked = true;
		intlock_unlock();
		return;
	}
	if (mutex->queue_head == NULL) {
		mutex->queue_head = mutex->queue_tail = process;
	} else {
		mutex->queue_tail->next = process;
	}
	process->next_in_queue = NULL;
	proc_pause_self(true);
}

void mutex_unlock(struct mutex *mutex) {
	if (!proc_is_initialized()) {
		return;
	}
	intlock_lock();
	if (mutex->queue_head == NULL) {
		mutex->locked = false;
		intlock_unlock();
		return;
	} else if (mutex->queue_head == mutex->queue_tail) {
		struct proc_process *process = mutex->queue_head;
		mutex->queue_head = mutex->queue_tail = NULL;
		intlock_unlock();
		proc_continue(process->pid);
	} else {
		struct proc_process *process = mutex->queue_head;
		mutex->queue_head = mutex->queue_head->next;
		intlock_unlock();
		proc_continue(process->pid);
	}
}

bool mutex_is_queued(struct mutex *mutex) {
	if (!proc_is_initialized()) {
		return false;
	}
	intlock_lock();
	bool result = mutex->queue_head != NULL;
	intlock_unlock();
	return result;
}

bool mutex_is_locked(struct mutex *mutex) {
	if (!proc_is_initialized()) {
		return false;
	}
	intlock_lock();
	bool result = mutex->locked;
	intlock_unlock();
	return result;
}
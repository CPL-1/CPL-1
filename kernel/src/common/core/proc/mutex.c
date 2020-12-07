#include <common/core/proc/mutex.h>
#include <common/core/proc/proc.h>
#include <common/core/proc/proclayout.h>
#include <common/lib/kmsg.h>
#include <hal/proc/intlock.h>

void Mutex_Initialize(struct Mutex *mutex) {
	mutex->queueHead = mutex->queueTail = NULL;
	mutex->locked = false;
}

void Mutex_Lock(struct Mutex *mutex) {
	if (!Proc_IsInitialized()) {
		return;
	}
	HAL_InterruptLock_Lock();
	struct Proc_Process *process = Proc_GetProcessData(Proc_GetProcessID());
	if (process == NULL) {
		KernelLog_ErrorMsg("Mutex Manager", "Failed to get current process data");
	}
	if (!(mutex->locked)) {
		mutex->locked = true;
		HAL_InterruptLock_Unlock();
		return;
	}
	if (mutex->queueHead == NULL) {
		mutex->queueHead = mutex->queueTail = process;
	} else {
		mutex->queueTail->nextInQueue = process;
	}
	process->nextInQueue = NULL;
	Proc_SuspendSelf(true);
}

void Mutex_Unlock(struct Mutex *mutex) {
	if (!Proc_IsInitialized()) {
		return;
	}
	HAL_InterruptLock_Lock();
	if (mutex->queueHead == NULL) {
		mutex->locked = false;
		HAL_InterruptLock_Unlock();
		return;
	} else if (mutex->queueHead == mutex->queueTail) {
		struct Proc_Process *process = mutex->queueTail;
		mutex->queueHead = mutex->queueTail = NULL;
		HAL_InterruptLock_Unlock();
		Proc_Resume(process->pid);
	} else {
		struct Proc_Process *process = mutex->queueHead;
		mutex->queueHead = mutex->queueHead->nextInQueue;
		HAL_InterruptLock_Unlock();
		Proc_Resume(process->pid);
	}
}

bool Mutex_IsAnyProcessWaiting(struct Mutex *mutex) {
	if (!Proc_IsInitialized()) {
		return false;
	}
	HAL_InterruptLock_Lock();
	bool result = mutex->queueHead != NULL;
	HAL_InterruptLock_Unlock();
	return result;
}

bool Mutex_IsLocked(struct Mutex *mutex) {
	if (!Proc_IsInitialized()) {
		return false;
	}
	HAL_InterruptLock_Lock();
	bool result = mutex->locked;
	HAL_InterruptLock_Unlock();
	return result;
}
#include <common/core/proc/mutex.h>
#include <common/core/proc/proc.h>
#include <common/core/proc/proclayout.h>
#include <common/lib/kmsg.h>
#include <hal/proc/intlevel.h>

void Mutex_Initialize(struct Mutex *mutex) {
	mutex->queueHead = mutex->queueTail = NULL;
	mutex->locked = false;
}

void Mutex_Lock(struct Mutex *mutex) {
	if (!Proc_IsInitialized()) {
		return;
	}
	int level = HAL_InterruptLevel_Elevate();
	struct Proc_Process *process = Proc_GetProcessData(Proc_GetProcessID());
	if (process == NULL) {
		KernelLog_ErrorMsg("Mutex Manager", "Failed to get current process data");
	}
	if (!(mutex->locked)) {
		mutex->locked = true;
		HAL_InterruptLevel_Recover(level);
		return;
	}
	if (mutex->queueHead == NULL) {
	Proc_SuspendSelf(true);
		mutex->queueHead = mutex->queueTail = process;
	} else {
		mutex->queueTail->nextInQueue = process;
	}
	process->nextInQueue = NULL;
}

void Mutex_Unlock(struct Mutex *mutex) {
	if (!Proc_IsInitialized()) {
		return;
	}
	int level = HAL_InterruptLevel_Elevate();
	if (mutex->queueHead == NULL) {
		mutex->locked = false;
		HAL_InterruptLevel_Recover(level);
		return;
	} else if (mutex->queueHead == mutex->queueTail) {
		struct Proc_Process *process = mutex->queueTail;
		mutex->queueHead = mutex->queueTail = NULL;
		Proc_Resume(process->pid);
		HAL_InterruptLevel_Recover(level);
	} else {
		struct Proc_Process *process = mutex->queueHead;
		mutex->queueHead = mutex->queueHead->nextInQueue;
		Proc_Resume(process->pid);
		HAL_InterruptLevel_Recover(level);
	}
}

bool Mutex_IsAnyProcessWaiting(struct Mutex *mutex) {
	if (!Proc_IsInitialized()) {
		return false;
	}
	int level = HAL_InterruptLevel_Elevate();
	bool result = mutex->queueHead != NULL;
	HAL_InterruptLevel_Recover(level);
	return result;
}

bool Mutex_IsLocked(struct Mutex *mutex) {
	if (!Proc_IsInitialized()) {
		return false;
	}
	int level = HAL_InterruptLevel_Elevate();
	bool result = mutex->locked;
	HAL_InterruptLevel_Recover(level);
	return result;
}
#ifndef __MUTEX_H_INCLUDED__
#define __MUTEX_H_INCLUDED__

#include <common/misc/utils.h>

struct Mutex {
	struct Proc_Process *queueHead;
	struct Proc_Process *queueTail;
	bool locked;
};

void Mutex_Initialize(struct Mutex *mutex);
void Mutex_Lock(struct Mutex *mutex);
void Mutex_Unlock(struct Mutex *mutex);
bool Mutex_IsAnyProcessWaiting(struct Mutex *mutex);
bool Mutex_IsLocked(struct Mutex *mutex);

#endif

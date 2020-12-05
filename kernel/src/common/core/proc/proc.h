#ifndef __PROC_H_INCLUDED__
#define __PROC_H_INCLUDED__

#define PROC_MAX_PROCESS_COUNT 4096
#define PROC_INVALID_PROC_ID                                                                                           \
	(struct Proc_ProcessID) {                                                                                          \
		.id = PROC_MAX_PROCESS_COUNT, .instance_number = 0                                                             \
	}

struct Proc_ProcessID {
	uint64_t id;
	uint64_t instance_number;
};

static INLINE bool proc_is_valid_Proc_ProcessID(struct Proc_ProcessID id) {
	return id.id != PROC_MAX_PROCESS_COUNT;
}

void Proc_Initialize();
bool Proc_IsInitialized();

struct Proc_ProcessID Proc_MakeNewProcess(struct Proc_ProcessID parent);

void Proc_Yield();
struct Proc_ProcessID Proc_GetProcessID();
void Proc_SuspendSelf(bool overrideState);
void Proc_Suspend(struct Proc_ProcessID id, bool overrideState);
void Proc_Resume(struct Proc_ProcessID id);
struct Proc_Process *Proc_WaitForChildTermination();
struct Proc_Process *Proc_GetProcessData(struct Proc_ProcessID id);

void Proc_Exit(int exitCode);
void Proc_Dispose(struct Proc_Process *process);

bool Proc_PollDisposeQueue();

#endif

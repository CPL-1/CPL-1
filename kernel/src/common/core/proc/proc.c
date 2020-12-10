#include <common/core/memory/heap.h>
#include <common/core/memory/virt.h>
#include <common/core/proc/proc.h>
#include <common/core/proc/proclayout.h>
#include <common/lib/kmsg.h>
#include <hal/memory/phys.h>
#include <hal/memory/virt.h>
#include <hal/proc/intlevel.h>
#include <hal/proc/isrhandler.h>
#include <hal/proc/stack.h>
#include <hal/proc/state.h>
#include <hal/proc/timer.h>

#define PROC_MOD_NAME "Process Manager & Scheduler"

static uint64_t m_instanceCountsByID[PROC_MAX_PROCESS_COUNT];
static struct Proc_Process *m_processesByID[PROC_MAX_PROCESS_COUNT];
static struct Proc_Process *m_CurrentProcess;
static struct Proc_Process *m_deallocQueueHead;
static bool m_procInitialized = false;

#define PROC_SCHEDULER_STACK_SIZE 65536
static char Proc_SchedulerStack[PROC_SCHEDULER_STACK_SIZE];

bool Proc_IsInitialized() {
	return m_procInitialized;
}

struct Proc_Process *Proc_GetProcessData(struct Proc_ProcessID id) {
	size_t array_index = id.id;
	if (array_index >= PROC_MAX_PROCESS_COUNT) {
		return NULL;
	}
	int level = HAL_InterruptLevel_Elevate();
	struct Proc_Process *data = m_processesByID[array_index];
	if (data == NULL) {
		HAL_InterruptLevel_Recover(level);
		return NULL;
	}
	if (m_instanceCountsByID[array_index] != id.instanceNumber) {
		HAL_InterruptLevel_Recover(level);
		return NULL;
	}
	HAL_InterruptLevel_Recover(level);
	return data;
}

static struct Proc_ProcessID Proc_AllocateProcessID(struct Proc_Process *process) {
	struct Proc_ProcessID result;
	for (size_t i = 0; i < PROC_MAX_PROCESS_COUNT; ++i) {
		int level = HAL_InterruptLevel_Elevate();
		if (m_processesByID[i] == NULL) {
			m_processesByID[i] = process;
			result.id = i;
			result.instanceNumber = m_instanceCountsByID[i];
			HAL_InterruptLevel_Recover(level);
			return result;
		}
		HAL_InterruptLevel_Recover(level);
	}
	result.id = PROC_MAX_PROCESS_COUNT;
	result.instanceNumber = 0;
	return result;
}

struct Proc_ProcessID Proc_MakeNewProcess(struct Proc_ProcessID parent) {
	struct Proc_Process *process = ALLOC_OBJ(struct Proc_Process);
	if (process == NULL) {
		goto fail;
	}
	uintptr_t stack = (uintptr_t)Heap_AllocateMemory(PROC_KERNEL_STACK_SIZE);
	if (stack == 0) {
		goto free_process_obj;
	}
	char *process_state = (char *)(Heap_AllocateMemory(HAL_PROCESS_STATE_SIZE));
	if (process_state == NULL) {
		goto free_stack;
	}
	struct Proc_ProcessID new_id = Proc_AllocateProcessID(process);
	if (!Proc_IsValidProcessID(new_id)) {
		goto free_process_state;
	}
	memset(process_state, 0, HAL_PROCESS_STATE_SIZE);
	process->next = process->prev = process->waitQueueHead = process->waitQueueTail = process->nextInQueue = NULL;
	process->ppid = parent;
	process->pid = new_id;
	process->processState = process_state;
	process->kernelStack = stack;
	process->returnCode = 0;
	process->state = SLEEPING;
	process->addressSpace = NULL;
	return new_id;
free_process_state:
	Heap_FreeMemory(process_state, HAL_PROCESS_STATE_SIZE);
free_stack:
	Heap_FreeMemory((void *)stack, PROC_KERNEL_STACK_SIZE);
free_process_obj:
	FREE_OBJ(process);
fail:;
	struct Proc_ProcessID failed_id;
	failed_id.instanceNumber = 0;
	failed_id.id = PROC_MAX_PROCESS_COUNT;
	return failed_id;
}

void Proc_VerifyProcess(struct Proc_Process *process) {
	if (process->next == process->prev && process->next == process) {
		return;
	}
	if ((process->next == process) || (process->prev == process)) {
		KernelLog_ErrorMsg("Process Manager & Scheduler", "Process references to itself with next and prev");
	}
}

void Proc_Resume(struct Proc_ProcessID id) {
	struct Proc_Process *process = Proc_GetProcessData(id);
	if (process == NULL) {
		return;
	}
	int level = HAL_InterruptLevel_Elevate();
	process->state = RUNNING;
	Proc_VerifyProcess(m_CurrentProcess);
	struct Proc_Process *next, *prev;
	next = m_CurrentProcess;
	prev = m_CurrentProcess->prev;
	next->prev = process;
	prev->next = process;
	process->next = next;
	process->prev = prev;
	Proc_VerifyProcess(process);
	Proc_VerifyProcess(next);
	Proc_VerifyProcess(prev);
	HAL_InterruptLevel_Recover(level);
}

static void Proc_CutFromActiveList(struct Proc_Process *process) {
	struct Proc_Process *prev = process->prev;
	struct Proc_Process *next = process->next;
	prev->next = next;
	next->prev = prev;
	Proc_VerifyProcess(process);
	Proc_VerifyProcess(next);
	Proc_VerifyProcess(prev);
}

void Proc_Suspend(struct Proc_ProcessID id, bool overrideState) {
	struct Proc_Process *process = Proc_GetProcessData(id);
	if (process == NULL) {
		return;
	}
	int level = HAL_InterruptLevel_Elevate();
	if (overrideState) {
		process->state = SLEEPING;
	}
	Proc_CutFromActiveList(process);
	if (process == m_CurrentProcess) {
		Proc_Yield();
	} else {
		HAL_InterruptLevel_Recover(level);
	}
}

struct Proc_ProcessID Proc_GetProcessID() {
	struct Proc_Process *current = m_CurrentProcess;
	struct Proc_ProcessID id = current->pid;
	return id;
}

void Proc_SuspendSelf(bool overrideState) {
	Proc_Suspend(Proc_GetProcessID(), overrideState);
}

void Proc_Dispose(struct Proc_Process *process) {
	int level = HAL_InterruptLevel_Elevate();
	process->nextInQueue = m_deallocQueueHead;
	m_deallocQueueHead = process;
	HAL_InterruptLevel_Recover(level);
}

void Proc_Exit(int exitCode) {
	HAL_InterruptLevel_Elevate();
	struct Proc_Process *process = m_CurrentProcess;
	process->returnCode = exitCode;
	m_instanceCountsByID[process->pid.id]++;
	m_processesByID[process->pid.id] = NULL;
	process->state = ZOMBIE;
	Proc_CutFromActiveList(process);
	struct Proc_ProcessID parentID = process->ppid;
	struct Proc_Process *parentProcess = Proc_GetProcessData(parentID);
	if (parentProcess == NULL) {
		Proc_Dispose(process);
	} else {
		if (parentProcess->waitQueueHead == NULL) {
			parentProcess->waitQueueHead = parentProcess->waitQueueTail = process;
		} else {
			parentProcess->waitQueueTail->nextInQueue = process;
		}
		process->nextInQueue = NULL;
		if (parentProcess->state == WAITING_FOR_CHILD_TERM) {
			Proc_Resume(parentID);
		}
	}
	Proc_Yield();
}

struct Proc_Process *Proc_GetWaitingQueueHead(struct Proc_Process *process) {
	struct Proc_Process *result = process->waitQueueHead;
	if (process->waitQueueTail == result) {
		process->waitQueueHead = process->waitQueueTail = NULL;
	} else {
		process->waitQueueHead = process->waitQueueHead->nextInQueue;
	}
	return result;
}

struct Proc_Process *Proc_WaitForChildTermination() {
	int level = HAL_InterruptLevel_Elevate();
	struct Proc_Process *process = m_CurrentProcess;
	if (process->waitQueueHead != NULL) {
		struct Proc_Process *result = Proc_GetWaitingQueueHead(process);
		HAL_InterruptLevel_Recover(level);
		return result;
	}
	process->state = WAITING_FOR_CHILD_TERM;
	Proc_SuspendSelf(false);
	struct Proc_Process *result = Proc_GetWaitingQueueHead(process);
	return result;
}

void Proc_Yield() {
	HAL_Timer_TriggerInterrupt();
}

void Proc_PreemptCallback(MAYBE_UNUSED void *ctx, char *frame) {
	memcpy(m_CurrentProcess->processState, frame, HAL_PROCESS_STATE_SIZE);
	m_CurrentProcess = m_CurrentProcess->next;
	memcpy(frame, m_CurrentProcess->processState, HAL_PROCESS_STATE_SIZE);
	VirtualMM_PreemptToAddressSpace(m_CurrentProcess->addressSpace);
	HAL_ISRStacks_SetSyscallsStack(m_CurrentProcess->kernelStack + PROC_KERNEL_STACK_SIZE);
}

void Proc_Initialize() {
	for (size_t i = 0; i < PROC_MAX_PROCESS_COUNT; ++i) {
		m_processesByID[i] = NULL;
		m_instanceCountsByID[i] = 0;
	}
	struct Proc_ProcessID kernelProcID = Proc_MakeNewProcess(PROC_INVALID_PROC_ID);
	if (!Proc_IsValidProcessID(kernelProcID)) {
		KernelLog_ErrorMsg(PROC_MOD_NAME, "Failed to allocate kernel process");
	}
	struct Proc_Process *kernelProcessData = Proc_GetProcessData(kernelProcID);
	if (kernelProcessData == NULL) {
		KernelLog_ErrorMsg(PROC_MOD_NAME, "Failed to access data of the kernel process");
	}
	kernelProcessData->state = RUNNING;
	m_CurrentProcess = kernelProcessData;
	kernelProcessData->next = kernelProcessData;
	kernelProcessData->prev = kernelProcessData;
	kernelProcessData->processState = Heap_AllocateMemory(HAL_PROCESS_STATE_SIZE);
	if (kernelProcessData->processState == NULL) {
		KernelLog_ErrorMsg(PROC_MOD_NAME, "Failed to allocate kernel process state");
	}
	kernelProcessData->addressSpace = VirtualMM_MakeAddressSpaceFromRoot(HAL_VirtualMM_GetCurrentAddressSpace());
	if (kernelProcessData->addressSpace == NULL) {
		KernelLog_ErrorMsg(PROC_MOD_NAME, "Failed to allocate process address space object");
	}
	m_deallocQueueHead = NULL;
	HAL_ISRStacks_SetISRStack((uintptr_t)(Proc_SchedulerStack) + PROC_SCHEDULER_STACK_SIZE);
	if (!HAL_Timer_SetCallback((HAL_ISR_Handler)Proc_PreemptCallback)) {
		KernelLog_ErrorMsg(PROC_MOD_NAME, "Failed to set timer callback");
	}
	m_procInitialized = true;
}

bool Proc_PollDisposeQueue() {
	int level = HAL_InterruptLevel_Elevate();
	struct Proc_Process *process = m_deallocQueueHead;
	if (process == NULL) {
		HAL_InterruptLevel_Recover(level);
		return false;
	}
	m_deallocQueueHead = process->nextInQueue;
	HAL_InterruptLevel_Recover(level);
	if (process->addressSpace != 0) {
		VirtualMM_DropAddressSpace(process->addressSpace);
	}
	if (process->kernelStack != 0) {
		Heap_FreeMemory((void *)(process->kernelStack), PROC_KERNEL_STACK_SIZE);
	}
	FREE_OBJ(process);
	return true;
}
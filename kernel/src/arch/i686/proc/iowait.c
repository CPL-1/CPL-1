#include <arch/i686/cpu/idt.h>
#include <arch/i686/drivers/pic.h>
#include <arch/i686/proc/iowait.h>
#include <arch/i686/proc/isrhandler.h>
#include <common/core/memory/heap.h>
#include <common/core/proc/proc.h>
#include <common/lib/kmsg.h>
#include <hal/proc/intlock.h>
#include <hal/proc/isrhandler.h>

struct i686_IOWait_ListEntry {
	i686_iowait_handler_t int_handler;
	i686_iowait_wakeup_handler_t check_wakeup_handler;
	void *ctx;
	struct i686_IOWait_ListEntry *next;
	struct Proc_ProcessID id;
};

struct i686_IOWait_IRQMeta {
	uint8_t irq;
};

struct i686_IOWait_ListEntry *i686_IOWait_HandlerLists[16];
struct i686_IOWait_IRQMeta i686_IOWait_IRQContexts[16];

void i686_IOWait_Initialize() {
	for (uint8_t i = 0; i < 16; ++i) {
		i686_IOWait_HandlerLists[i] = NULL;
		i686_IOWait_IRQContexts[i].irq = i;
	}
}

void i686_IOWait_HandleIRQ(void *ctx, void *frame) {
	struct i686_IOWait_IRQMeta *meta = (struct i686_IOWait_IRQMeta *)ctx;
	uint8_t irq = meta->irq;
	struct i686_IOWait_ListEntry *head = i686_IOWait_HandlerLists[irq];
	while (head != NULL) {
		if (head->check_wakeup_handler == NULL || head->check_wakeup_handler(head->ctx)) {
			if (head->int_handler != NULL) {
				head->int_handler(head->ctx, frame);
			}
			if (proc_is_valid_Proc_ProcessID(head->id)) {
				Proc_Resume(head->id);
			}
			break;
		}
		head = head->next;
	}
	i686_PIC8259_NotifyOnIRQTerm(irq);
}

struct i686_IOWait_ListEntry *i686_IOWait_AddHandler(uint8_t irq, i686_iowait_handler_t int_handler,
													 i686_iowait_wakeup_handler_t check_hander, void *ctx) {
	struct i686_IOWait_ListEntry *entry = ALLOC_OBJ(struct i686_IOWait_ListEntry);
	if (entry == NULL) {
		KernelLog_ErrorMsg("i686 IO wait subsystem", "Failed to allocate object for IOWait handler");
	}
	entry->int_handler = int_handler;
	entry->check_wakeup_handler = check_hander;
	entry->ctx = ctx;
	if (i686_IOWait_HandlerLists[irq] == NULL) {
		void *interrupt_handler =
			i686_ISR_MakeNewISRHandler((HAL_ISR_Handler)i686_IOWait_HandleIRQ, i686_IOWait_IRQContexts + irq);
		if (interrupt_handler == NULL) {
			KernelLog_ErrorMsg("i686 IO wait subsystem", "Failed to allocate function object for IRQ handler");
		}
		i686_IDT_InstallISR(irq + 0x20, (uint32_t)interrupt_handler);
		i686_PIC8259_EnableIRQ(irq);
	}
	entry->id = PROC_INVALID_PROC_ID;
	entry->next = i686_IOWait_HandlerLists[irq];
	i686_IOWait_HandlerLists[irq] = entry;
	return entry;
}

void i686_IOWait_WaitForIRQ(struct i686_IOWait_ListEntry *entry) {
	HAL_InterruptLock_Lock();
	struct Proc_ProcessID id = Proc_GetProcessID();
	entry->id = id;
	Proc_Suspend(id, true);
}

void i686_IOWait_UnmaskUsedIRQ() {
	for (size_t i = 0; i < 16; ++i) {
		if (i686_IOWait_HandlerLists[i] != NULL) {
			i686_PIC8259_EnableIRQ(i);
		}
	}
	HAL_InterruptLock_Flush();
}

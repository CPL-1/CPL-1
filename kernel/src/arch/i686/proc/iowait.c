#include <arch/i686/cpu/idt.h>
#include <arch/i686/drivers/pic.h>
#include <arch/i686/proc/iowait.h>
#include <arch/i686/proc/isrhandler.h>
#include <common/core/memory/heap.h>
#include <common/core/proc/proc.h>
#include <common/lib/kmsg.h>
#include <hal/proc/intlevel.h>
#include <hal/proc/isrhandler.h>

struct i686_IOWait_ListEntry {
	i686_IOWait_Handler int_handler;
	i686_IOWait_WakeupHandler check_wakeup_handler;
	void *ctx;
	struct i686_IOWait_ListEntry *next;
	struct Proc_ProcessID id;
};

struct i686_IOWait_IRQMeta {
	uint8_t irq;
};

struct i686_IOWait_ListEntry *m_handlerLists[16];
struct i686_IOWait_IRQMeta m_irqContexts[16];

void i686_IOWait_Initialize() {
	for (uint8_t i = 0; i < 16; ++i) {
		m_handlerLists[i] = NULL;
		m_irqContexts[i].irq = i;
	}
}

void i686_IOWait_HandleIRQ(void *ctx, void *frame) {
	struct i686_IOWait_IRQMeta *meta = (struct i686_IOWait_IRQMeta *)ctx;
	uint8_t irq = meta->irq;
	struct i686_IOWait_ListEntry *head = m_handlerLists[irq];
	while (head != NULL) {
		if (head->check_wakeup_handler == NULL || head->check_wakeup_handler(head->ctx)) {
			if (head->int_handler != NULL) {
				head->int_handler(head->ctx, frame);
			}
			if (Proc_IsValidProcessID(head->id)) {
				Proc_Resume(head->id);
				head->id = PROC_INVALID_PROC_ID;
			}
			break;
		}
		head = head->next;
	}
	i686_PIC8259_NotifyOnIRQTerm(irq);
}

struct i686_IOWait_ListEntry *i686_IOWait_AddHandler(uint8_t irq, i686_IOWait_Handler int_handler,
													 i686_IOWait_WakeupHandler check_hander, void *ctx) {
	struct i686_IOWait_ListEntry *entry = ALLOC_OBJ(struct i686_IOWait_ListEntry);
	if (entry == NULL) {
		KernelLog_ErrorMsg("i686 IO wait subsystem", "Failed to allocate object for IOWait handler");
	}
	entry->int_handler = int_handler;
	entry->check_wakeup_handler = check_hander;
	entry->ctx = ctx;
	entry->id = PROC_INVALID_PROC_ID;
	entry->next = m_handlerLists[irq];
	m_handlerLists[irq] = entry;
	if (entry->next == NULL) {
		void *interrupt_handler =
			i686_ISR_MakeNewISRHandler((HAL_ISR_Handler)i686_IOWait_HandleIRQ, m_irqContexts + irq, false);
		if (interrupt_handler == NULL) {
			KernelLog_ErrorMsg("i686 IO wait subsystem", "Failed to allocate function object for IRQ handler");
		}
		i686_IDT_InstallISR(irq + 0x20, (uint32_t)interrupt_handler);
		i686_PIC8259_EnableIRQ(irq);
	}
	return entry;
}

void i686_IOWait_WaitForIRQ(struct i686_IOWait_ListEntry *entry) {
	HAL_InterruptLevel_Elevate();
	struct Proc_ProcessID id = Proc_GetProcessID();
	entry->id = id;
	Proc_Suspend(id, true);
}
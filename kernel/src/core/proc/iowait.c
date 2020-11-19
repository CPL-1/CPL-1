#include <arch/i386/idt.h>
#include <core/memory/heap.h>
#include <core/proc/intlock.h>
#include <core/proc/iowait.h>
#include <core/proc/proc.h>
#include <drivers/pic.h>
#include <lib/fembed.h>
#include <lib/kmsg.h>

struct iowait_list_entry {
	iowait_handler_t int_handler;
	iowait_wakeup_handler_t check_wakeup_handler;
	void *ctx;
	struct iowait_list_entry *next;
	struct proc_id id;
};

struct iowait_irq_meta {
	uint8_t irq;
};

struct iowait_list_entry *iowait_handler_lists[16];
struct iowait_irq_meta iowait_irq_contexts[16];

void iowait_init() {
	for (uint8_t i = 0; i < 16; ++i) {
		iowait_handler_lists[i] = NULL;
		iowait_irq_contexts[i].irq = i;
	}
}

void iowait_interrupt_handler(void *ctx, void *frame) {
	struct iowait_irq_meta *meta = (struct iowait_irq_meta *)ctx;
	uint8_t irq = meta->irq;
	struct iowait_list_entry *head = iowait_handler_lists[irq];
	while (head != NULL) {
		if (head->check_wakeup_handler == NULL ||
			head->check_wakeup_handler(head->ctx)) {
			if (head->int_handler != NULL) {
				head->int_handler(head->ctx, frame);
			}
			if (proc_is_valid_proc_id(head->id)) {
				proc_continue(head->id);
			}
			break;
		}
		head = head->next;
	}
	pic_irq_notify_on_term(irq);
}

struct iowait_list_entry *
iowait_add_handler(uint8_t irq, iowait_handler_t int_handler,
				   iowait_wakeup_handler_t check_hander, void *ctx) {
	struct iowait_list_entry *entry = ALLOC_OBJ(struct iowait_list_entry);
	if (entry == NULL) {
		kmsg_err("IO wait subsystem",
				 "Failed to allocate object for IOWait handler");
	}
	entry->int_handler = int_handler;
	entry->check_wakeup_handler = check_hander;
	entry->ctx = ctx;
	if (iowait_handler_lists[irq] == NULL) {
		void *interrupt_handler = fembed_make_irq_handler(
			iowait_interrupt_handler, iowait_irq_contexts + irq);
		if (interrupt_handler == NULL) {
			kmsg_err("IO wait subsystem",
					 "Failed to allocate function object for IRQ handler");
		}
		idt_install_isr(irq + 0x20, (uint32_t)interrupt_handler);
	}
	entry->id = INVALID_PROC_ID;
	entry->next = iowait_handler_lists[irq];
	iowait_handler_lists[irq] = entry;
	return entry;
}

void iowait_wait(struct iowait_list_entry *entry) {
	intlock_lock();
	struct proc_id id = proc_my_id();
	entry->id = id;
	proc_pause(id, true);
}

void iowait_enable_used_irq() {
	for (size_t i = 0; i < 16; ++i) {
		if (iowait_handler_lists[i] != NULL) {
			pic_irq_enable(i);
		}
	}
	pic_irq_disable(0);
	intlock_flush();
}

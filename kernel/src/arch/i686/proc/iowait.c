#include <arch/i686/cpu/idt.h>
#include <arch/i686/drivers/pic.h>
#include <arch/i686/proc/iowait.h>
#include <arch/i686/proc/isrhandler.h>
#include <common/core/memory/heap.h>
#include <common/core/proc/proc.h>
#include <common/lib/kmsg.h>
#include <hal/proc/intlock.h>
#include <hal/proc/isrhandler.h>

struct i686_iowait_list_entry {
	i686_iowait_handler_t int_handler;
	i686_iowait_wakeup_handler_t check_wakeup_handler;
	void *ctx;
	struct i686_iowait_list_entry *next;
	struct proc_id id;
};

struct i686_iowait_irq_meta {
	uint8_t irq;
};

struct i686_iowait_list_entry *i686_iowait_handler_lists[16];
struct i686_iowait_irq_meta i686_iowait_irq_contexts[16];

void i686_iowait_init() {
	for (uint8_t i = 0; i < 16; ++i) {
		i686_iowait_handler_lists[i] = NULL;
		i686_iowait_irq_contexts[i].irq = i;
	}
}

void i686_iowait_interrupt_handler(void *ctx, void *frame) {
	struct i686_iowait_irq_meta *meta = (struct i686_iowait_irq_meta *)ctx;
	uint8_t irq = meta->irq;
	struct i686_iowait_list_entry *head = i686_iowait_handler_lists[irq];
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
	i686_pic_irq_notify_on_term(irq);
}

struct i686_iowait_list_entry *
i686_iowait_add_handler(uint8_t irq, i686_iowait_handler_t int_handler,
						i686_iowait_wakeup_handler_t check_hander, void *ctx) {
	struct i686_iowait_list_entry *entry =
		ALLOC_OBJ(struct i686_iowait_list_entry);
	if (entry == NULL) {
		kmsg_err("i686 IO wait subsystem",
				 "Failed to allocate object for IOWait handler");
	}
	entry->int_handler = int_handler;
	entry->check_wakeup_handler = check_hander;
	entry->ctx = ctx;
	if (i686_iowait_handler_lists[irq] == NULL) {
		void *interrupt_handler = i686_isr_handler_new(
			(hal_isr_handler_t)i686_iowait_interrupt_handler,
			i686_iowait_irq_contexts + irq);
		if (interrupt_handler == NULL) {
			kmsg_err("i686 IO wait subsystem",
					 "Failed to allocate function object for IRQ handler");
		}
		i686_idt_install_isr(irq + 0x20, (uint32_t)interrupt_handler);
	}
	entry->id = INVALID_PROC_ID;
	entry->next = i686_iowait_handler_lists[irq];
	i686_iowait_handler_lists[irq] = entry;
	return entry;
}

void i686_iowait_wait(struct i686_iowait_list_entry *entry) {
	hal_intlock_lock();
	struct proc_id id = proc_my_id();
	entry->id = id;
	proc_pause(id, true);
}

void i686_iowait_enable_used_irq() {
	for (size_t i = 0; i < 16; ++i) {
		if (i686_iowait_handler_lists[i] != NULL) {
			i686_pic_irq_enable(i);
		}
	}
	hal_intlock_flush();
}

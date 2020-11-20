#ifndef __IOWAIT_H_INCLUDED__
#define __IOWAIT_H_INCLUDED__

#include <common/misc/utils.h>
#include <hal/proc/isrhandler.h>

typedef hal_isr_handler_t iowait_handler_t;
typedef bool (*iowait_wakeup_handler_t)(void *ctx);

void iowait_init();
struct iowait_list_entry *
iowait_add_handler(uint8_t irq, iowait_handler_t int_handler,
				   iowait_wakeup_handler_t check_hander, void *ctx);
void iowait_wait(struct iowait_list_entry *entry);
void iowait_enable_used_irq();

#endif
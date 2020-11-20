#ifndef __IOWAIT_H_INCLUDED__
#define __IOWAIT_H_INCLUDED__

#include <common/misc/utils.h>
#include <hal/proc/isrhandler.h>

typedef hal_isr_handler_t i686_iowait_handler_t;
typedef bool (*i686_iowait_wakeup_handler_t)(void *ctx);

void i686_iowait_init();
struct i686_iowait_list_entry *
i686_iowait_add_handler(uint8_t irq, i686_iowait_handler_t int_handler,
						i686_iowait_wakeup_handler_t check_hander, void *ctx);
void i686_iowait_wait(struct i686_iowait_list_entry *entry);
void i686_iowait_enable_used_irq();

#endif
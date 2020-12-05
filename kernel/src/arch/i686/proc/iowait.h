#ifndef __IOWAIT_H_INCLUDED__
#define __IOWAIT_H_INCLUDED__

#include <common/misc/utils.h>
#include <hal/proc/isrhandler.h>

typedef HAL_ISR_Handler i686_iowait_handler_t;
typedef bool (*i686_iowait_wakeup_handler_t)(void *ctx);

void i686_IOWait_Initialize();
struct i686_IOWait_ListEntry *i686_IOWait_AddHandler(UINT8 irq, i686_iowait_handler_t int_handler,
													 i686_iowait_wakeup_handler_t check_hander, void *ctx);
void i686_IOWait_WaitForIRQ(struct i686_IOWait_ListEntry *entry);
void i686_IOWait_UnmaskUsedIRQ();

#endif
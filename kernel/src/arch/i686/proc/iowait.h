#ifndef __IOWAIT_H_INCLUDED__
#define __IOWAIT_H_INCLUDED__

#include <common/misc/utils.h>
#include <hal/proc/isrhandler.h>

typedef HAL_ISR_Handler i686_IOWait_Handler;
typedef bool (*i686_IOWait_WakeupHandler)(void *ctx);

void i686_IOWait_Initialize();
struct i686_IOWait_ListEntry *i686_IOWait_AddHandler(uint8_t irq, i686_IOWait_Handler int_handler,
													 i686_IOWait_WakeupHandler check_hander, void *ctx);
void i686_IOWait_WaitForIRQ(struct i686_IOWait_ListEntry *entry);

#endif
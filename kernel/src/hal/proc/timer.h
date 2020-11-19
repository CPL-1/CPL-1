#ifndef __HAL_TIMER_H_INCLUDED__
#define __HAL_TIMER_H_INCLUDED__

#include <hal/proc/isrhandler.h>

bool hal_timer_set_callback(hal_isr_handler_t handler);
void hal_timer_trigger_callback();

#endif
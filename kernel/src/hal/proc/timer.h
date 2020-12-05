#ifndef __HAL_TIMER_H_INCLUDED__
#define __HAL_TIMER_H_INCLUDED__

#include <hal/proc/isrhandler.h>

bool HAL_Timer_SetCallback(HAL_ISR_Handler handler);
void HAL_Timer_TriggerInterrupt();

#endif
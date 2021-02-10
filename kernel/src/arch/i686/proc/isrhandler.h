#ifndef __I686_ISR_HANDLER_H_INCLUDED__
#define __I686_ISR_HANDLER_H_INCLUDED__

#include <hal/proc/isrhandler.h>

HAL_ISR_Handler i686_ISR_MakeNewISRHandler(HAL_ISR_Handler entry_point, void *ctx, bool errorCode);

#endif

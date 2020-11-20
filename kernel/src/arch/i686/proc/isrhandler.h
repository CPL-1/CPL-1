#ifndef __I686_ISR_HANDLER_H_INCLUDED__
#define __I686_ISR_HANDLER_H_INCLUDED__

#include <hal/proc/isrhandler.h>

hal_isr_handler_t i686_isr_handler_new(hal_isr_handler_t entry_point,
									   void *ctx);

#endif
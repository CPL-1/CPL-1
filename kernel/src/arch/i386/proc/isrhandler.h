#ifndef __I386_ISR_HANDLER_H_INCLUDED__
#define __I386_ISR_HANDLER_H_INCLUDED__

#include <hal/proc/isrhandler.h>

hal_isr_handler_t i386_isr_handler_new(hal_isr_handler_t entry_point,
									   void *ctx);

#endif
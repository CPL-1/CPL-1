#ifndef __HAL_ISRHANDLER_H_INCLUDED__
#define __HAL_ISRHANDLER_H_INCLUDED__

typedef void (*hal_isr_handler_t)(void *ctx, char *state);

#endif
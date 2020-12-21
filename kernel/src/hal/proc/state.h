#ifndef __HAL_STATE_H_INCLUDED__
#define __HAL_STATE_H_INCLUDED__

#include <common/misc/utils.h>

extern size_t HAL_PROCESS_STATE_SIZE;

void HAL_State_EnableInterrupts(void *state);
void HAL_State_DisableInterrupts(void *state);
void HAL_State_UpdateIP(void *state, uintptr_t ip);

#endif
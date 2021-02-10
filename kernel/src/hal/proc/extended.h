#ifndef __HAL_EXTENDED_H_INCLUDED__
#define __HAL_EXTENDED_H_INCLUDED__

#include <common/misc/utils.h>

extern size_t HAL_ExtendedStateSize;

void HAL_ExtendedState_LoadFrom(char *buf);
void HAL_ExtendedState_StoreTo(char *buf);

#endif

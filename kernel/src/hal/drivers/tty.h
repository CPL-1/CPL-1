#ifndef __HAL_LOG_H_INCLUDED__
#define __HAL_LOG_H_INCLUDED__

#include <common/misc/utils.h>

void HAL_TTY_PrintCharacter(char c);
void HAL_TTY_Flush();
void HAL_TTY_SetForegroundColor(uint8_t color);
void HAL_TTY_SetBackgroundColor(uint8_t color);
void HAL_TTY_Clear();

#endif
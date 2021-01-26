#ifndef __HAL_LOG_H_INCLUDED__
#define __HAL_LOG_H_INCLUDED__

#include <common/misc/utils.h>
#include <hal/proc/isrhandler.h>

void HAL_TTY_PrintCharacter(char c);
void HAL_TTY_Flush();
void HAL_TTY_SetForegroundColor(uint8_t color);
void HAL_TTY_SetBackgroundColor(uint8_t color);
void HAL_TTY_Clear();

void HAL_TTY_SetKeyboardPressHandler(HAL_ISR_Handler handler);
uint8_t HAL_TTY_GetLastPressedKey();

#endif
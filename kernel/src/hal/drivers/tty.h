#ifndef __HAL_LOG_H_INCLUDED__
#define __HAL_LOG_H_INCLUDED__

#include <common/misc/utils.h>
#include <hal/proc/isrhandler.h>

void HAL_TTY_PrintCharacter(char c);
void HAL_TTY_Flush();
void HAL_TTY_SetForegroundColor(uint8_t color);
void HAL_TTY_SetBackgroundColor(uint8_t color);
void HAL_TTY_Clear();

struct HAL_TTY_KeyEvent {
	char character;
	uint8_t raw;
	bool pressed;
	bool typeable;
};

void HAL_TTY_FlushKeyEventQueue();
void HAL_TTY_WaitForNextKeyEvent(struct HAL_TTY_KeyEvent *event);

#endif

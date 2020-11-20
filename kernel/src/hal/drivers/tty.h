#ifndef __HAL_LOG_H_INCLUDED__
#define __HAL_LOG_H_INCLUDED__

#include <utils/utils.h>

void hal_tty_putc(char c);
void hal_tty_flush();
void hal_tty_set_color(uint8_t color);
void hal_tty_clear();

#endif
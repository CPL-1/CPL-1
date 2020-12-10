#ifndef __HAL_INTLEVEL_H_INCLUDED__
#define __HAL_INTLEVEL_H_INCLUDED__

int HAL_InterruptLevel_Elevate();
void HAL_InterruptLevel_Recover(int status);

#endif
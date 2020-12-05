#ifndef __HAL_INTLOCK_H_INCLUDED__
#define __HAL_INTLOCK_H_INCLUDED__

void HAL_InterruptLock_Lock();
void HAL_InterruptLock_Unlock();
void HAL_InterruptLock_Flush();

#endif
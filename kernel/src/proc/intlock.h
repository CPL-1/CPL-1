#ifndef __INTLOCK_H_INCLUDED__
#define __INTLOCK_H_INCLUDED__

#include <utils.h>

void intlock_lock();
void intlock_unlock();
void intlock_flush();

#endif

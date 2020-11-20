#ifndef __PIT_H_INCLUDED__
#define __PIT_H_INCLUDED__

#include <common/misc/utils.h>
#include <hal/proc/timer.h>

void i686_pit_init(uint32_t freq);

#endif

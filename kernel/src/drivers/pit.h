#ifndef __PIT_H_INCLUDED__
#define __PIT_H_INCLUDED__

#include <utils.h>

void pit_init(uint32_t freq);
void pit_set_callback(uint32_t entry);
void pit_trigger_interrupt();

#endif

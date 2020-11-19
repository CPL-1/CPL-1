#ifndef __FEMBED_H_INCLUDED__
#define __FEMBED_H_INCLUDED__

#include <utils/utils.h>

void *fembed_make_irq_handler(void *func, void *arg);
size_t fembed_size();

#endif
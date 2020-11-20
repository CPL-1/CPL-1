#ifndef __I686_CR3_H_INCLUDED__
#define __I686_CR3_H_INCLUDED__

#include <common/misc/utils.h>

void i686_cr3_init();
void i686_cr3_set(uint32_t val);
uint32_t i686_cr3_get();

#endif
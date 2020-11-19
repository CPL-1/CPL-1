#ifndef __I386_CR3_H_INCLUDED__
#define __I386_CR3_H_INCLUDED__

#include <utils/utils.h>

void i386_cr3_init();
void i386_cr3_set(uint32_t val);
uint32_t i386_cr3_get();

#endif
#ifndef __I686_TSS_H_INCLUDED__
#define __I686_TSS_H_INCLUDED__

#include <common/misc/utils.h>

void i686_tss_init();
uint32_t i686_tss_get_base();
uint32_t i686_tss_get_limit();
void i686_tss_set_dpl0_stack(uint32_t esp, uint16_t ss);
void i686_tss_set_dpl1_stack(uint32_t esp, uint16_t ss);

#endif
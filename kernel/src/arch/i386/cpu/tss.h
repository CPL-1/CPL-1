#ifndef __I386_TSS_H_INCLUDED__
#define __I386_TSS_H_INCLUDED__

#include <common/misc/utils.h>

void i386_tss_init();
uint32_t i386_tss_get_base();
uint32_t i386_tss_get_limit();
void i386_tss_set_dpl0_stack(uint32_t esp, uint16_t ss);
void i386_tss_set_dpl1_stack(uint32_t esp, uint16_t ss);

#endif
#ifndef __TSS_H_INCLUDED__
#define __TSS_H_INCLUDED__

#include <utils.h>

void tss_init();
uint32_t tss_get_base();
uint32_t tss_get_limit();
void tss_set_dpl0_stack(uint32_t esp, uint16_t ss);
void tss_set_dpl1_stack(uint32_t esp, uint16_t ss);

#endif

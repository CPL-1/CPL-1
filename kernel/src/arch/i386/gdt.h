#ifndef __I386_GDT_H_INCLUDED__
#define __I386_GDT_H_INCLUDED__

#include <utils/utils.h>

void gdt_init();
uint16_t gdt_get_tss_segment();

#endif

#ifndef __I386_GDT_H_INCLUDED__
#define __I386_GDT_H_INCLUDED__

#include <utils/utils.h>

void i386_gdt_init();
uint16_t i386_gdt_get_tss_segment();

#endif

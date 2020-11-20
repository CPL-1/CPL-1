#ifndef __I686_GDT_H_INCLUDED__
#define __I686_GDT_H_INCLUDED__

#include <common/misc/utils.h>

void i686_gdt_init();
uint16_t i686_gdt_get_tss_segment();

#endif

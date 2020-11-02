#ifndef __GDT_H_INCLUDED__
#define __GDT_H_INCLUDED__

#include <utils.h>

void gdt_init();
uint16_t gdt_get_tss_segment();

#endif
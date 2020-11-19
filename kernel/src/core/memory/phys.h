#ifndef __PHYSICAL_H_INCLUDED__
#define __PHYSICAL_H_INCLUDED__

#include <utils/utils.h>

void phys_init();
uint32_t phys_hi_alloc_frame();
uint32_t phys_lo_alloc_frame();
void phys_free_frame(uint32_t frame);

uint32_t phys_lo_alloc_area(uint32_t size);
void phys_lo_free_area(uint32_t start, uint32_t size);

#endif

#ifndef __I686_MEMORY_PHYSICAL_H_INCLUDED__
#define __I686_MEMORY_PHYSICAL_H_INCLUDED__

#include <common/misc/utils.h>

void i686_phys_init();

uint32_t i686_phys_krnl_alloc_frame();
void i686_phys_krnl_free_frame(uint32_t frame);

#endif

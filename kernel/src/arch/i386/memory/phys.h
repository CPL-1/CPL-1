#ifndef __I386_MEMORY_PHYSICAL_H_INCLUDED__
#define __I386_MEMORY_PHYSICAL_H_INCLUDED__

#include <common/misc/utils.h>

void i386_phys_init();

uint32_t i386_phys_krnl_alloc_frame();
void i386_phys_krnl_free_frame(uint32_t frame);

#endif

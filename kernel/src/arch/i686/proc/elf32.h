#ifndef __I686_ELF32_H_INCLUDED__
#define __I686_ELF32_H_INCLUDED__

#include <common/core/proc/elf32.h>

bool i686_Elf32_HeaderVerifyCallback(struct Elf32_Header *header);

#endif
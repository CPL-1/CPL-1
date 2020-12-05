#ifndef __I686_GDT_H_INCLUDED__
#define __I686_GDT_H_INCLUDED__

#include <common/misc/utils.h>

void i686_GDT_Initialize();
UINT16 i686_GDT_GetTSSSegment();

#endif

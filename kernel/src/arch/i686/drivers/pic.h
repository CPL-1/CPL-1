#ifndef __I686_PIC_H_INCLUDED__
#define __I686_PIC_H_INCLUDED__

#include <common/misc/utils.h>

void i686_PIC8259_Initialize();
void i686_PIC8259_NotifyOnIRQTerm(uint8_t no);
void i686_PIC8259_EnableIRQ(uint8_t no);
void i686_PIC8259_DisableIRQ(uint8_t no);

#endif

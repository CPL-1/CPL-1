#ifndef __PIC_H_INCLUDED__
#define __PIC_H_INCLUDED__

#include <common/misc/utils.h>

void i686_PIC8259_Initialize();
void i686_PIC8259_NotifyOnIRQTerm(UINT8 no);
void i686_PIC8259_EnableIRQ(UINT8 no);
void i686_PIC8259_DisableIRQ(UINT8 no);

#endif

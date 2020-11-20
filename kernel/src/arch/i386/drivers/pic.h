#ifndef __PIC_H_INCLUDED__
#define __PIC_H_INCLUDED__

#include <common/misc/utils.h>

void i386_pic_init();
void i386_pic_irq_notify_on_term(uint8_t no);
void i386_pic_irq_enable(uint8_t no);
void i386_pic_irq_disable(uint8_t no);

#endif

#ifndef __PIC_H_INCLUDED__
#define __PIC_H_INCLUDED__

#include <common/misc/utils.h>

void i686_pic_init();
void i686_pic_irq_notify_on_term(uint8_t no);
void i686_pic_irq_enable(uint8_t no);
void i686_pic_irq_disable(uint8_t no);

#endif

#ifndef __PIC_H_INCLUDED__
#define __PIC_H_INCLUDED__

#include <utils.h>

void pic_init();
void pic_irq_notify_on_term(uint8_t no);
void pic_irq_enable(uint8_t no);
void pic_irq_disable(uint8_t no);

#endif

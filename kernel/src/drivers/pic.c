#include <drivers/pic.h>
#include <i386/cpu.h>
#include <i386/ports.h>

#define PIC1 0x20
#define PIC2 0xA0
#define PIC1_COMMAND PIC1
#define PIC1_DATA (PIC1 + 1)
#define PIC2_COMMAND PIC2
#define PIC2_DATA (PIC2 + 1)
#define PIC_EOI 0x20

#define ICW1_ICW4 0x01
#define ICW1_INIT 0x10
#define ICW4_8086 0x01

static uint8_t pic1_mask = 0xff;
static uint8_t pic2_mask = 0xff;

void pic_init() {
	outb(PIC1_COMMAND, ICW1_INIT | ICW1_ICW4);
	cpu_io_wait();
	outb(PIC2_COMMAND, ICW1_INIT | ICW1_ICW4);
	cpu_io_wait();
	outb(PIC1_DATA, 0x20);
	cpu_io_wait();
	outb(PIC2_DATA, 0x28);
	cpu_io_wait();
	outb(PIC1_DATA, 4);
	cpu_io_wait();
	outb(PIC2_DATA, 2);
	cpu_io_wait();
	outb(PIC1_DATA, ICW4_8086);
	cpu_io_wait();
	outb(PIC2_DATA, ICW4_8086);
	cpu_io_wait();
	outb(PIC1_DATA, pic1_mask);
	outb(PIC2_DATA, pic2_mask);
	asm volatile("sti");
}

void pic_irq_notify_on_term(uint8_t no) {
	if (no >= 8) {
		outb(PIC2_COMMAND, PIC_EOI);
	}
	outb(PIC1_COMMAND, PIC_EOI);
}

void pic_irq_enable(uint8_t no) {
	if (no > 8) {
		no -= 8;
		pic2_mask &= ~(1 << no);
	} else {
		pic1_mask &= ~(1 << no);
	}
	outb(PIC2_DATA, pic2_mask);
	outb(PIC1_DATA, pic1_mask);
}

void pic_irq_disable(uint8_t no) {
	if (no > 8) {
		no -= 8;
		pic2_mask |= (1 << no);
	} else {
		pic1_mask |= (1 << no);
	}
	outb(PIC2_DATA, pic2_mask);
	outb(PIC1_DATA, pic1_mask);
}

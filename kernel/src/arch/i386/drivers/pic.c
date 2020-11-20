#include <arch/i386/cpu/cpu.h>
#include <arch/i386/cpu/ports.h>
#include <arch/i386/drivers/pic.h>

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

void i386_pic_init() {
	outb(PIC1_COMMAND, ICW1_INIT | ICW1_ICW4);
	i386_cpu_io_wait();
	outb(PIC2_COMMAND, ICW1_INIT | ICW1_ICW4);
	i386_cpu_io_wait();
	outb(PIC1_DATA, 0x20);
	i386_cpu_io_wait();
	outb(PIC2_DATA, 0x28);
	i386_cpu_io_wait();
	outb(PIC1_DATA, 4);
	i386_cpu_io_wait();
	outb(PIC2_DATA, 2);
	i386_cpu_io_wait();
	outb(PIC1_DATA, ICW4_8086);
	i386_cpu_io_wait();
	outb(PIC2_DATA, ICW4_8086);
	i386_cpu_io_wait();
	outb(PIC1_DATA, pic1_mask);
	outb(PIC2_DATA, pic2_mask);
}

void i386_pic_irq_notify_on_term(uint8_t no) {
	if (no >= 8) {
		outb(PIC2_COMMAND, PIC_EOI);
	}
	outb(PIC1_COMMAND, PIC_EOI);
}

void i386_pic_irq_enable(uint8_t no) {
	if (no > 8) {
		no -= 8;
		pic2_mask &= ~(1 << no);
	} else {
		pic1_mask &= ~(1 << no);
	}
	outb(PIC2_DATA, pic2_mask);
	outb(PIC1_DATA, pic1_mask);
}

void i386_pic_irq_disable(uint8_t no) {
	if (no > 8) {
		no -= 8;
		pic2_mask |= (1 << no);
	} else {
		pic1_mask |= (1 << no);
	}
	outb(PIC2_DATA, pic2_mask);
	outb(PIC1_DATA, pic1_mask);
}

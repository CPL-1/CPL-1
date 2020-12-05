#include <arch/i686/cpu/cpu.h>
#include <arch/i686/cpu/ports.h>
#include <arch/i686/drivers/pic.h>

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

static uint8_t m_primaryPICMask = 0xff;
static uint8_t m_secondaryPICMask = 0xff;

void i686_PIC8259_Initialize() {
	i686_Ports_WriteByte(PIC1_COMMAND, ICW1_INIT | ICW1_ICW4);
	i686_CPU_WaitForIOCompletition();
	i686_Ports_WriteByte(PIC2_COMMAND, ICW1_INIT | ICW1_ICW4);
	i686_CPU_WaitForIOCompletition();
	i686_Ports_WriteByte(PIC1_DATA, 0x20);
	i686_CPU_WaitForIOCompletition();
	i686_Ports_WriteByte(PIC2_DATA, 0x28);
	i686_CPU_WaitForIOCompletition();
	i686_Ports_WriteByte(PIC1_DATA, 4);
	i686_CPU_WaitForIOCompletition();
	i686_Ports_WriteByte(PIC2_DATA, 2);
	i686_CPU_WaitForIOCompletition();
	i686_Ports_WriteByte(PIC1_DATA, ICW4_8086);
	i686_CPU_WaitForIOCompletition();
	i686_Ports_WriteByte(PIC2_DATA, ICW4_8086);
	i686_CPU_WaitForIOCompletition();
	i686_Ports_WriteByte(PIC1_DATA, m_primaryPICMask);
	i686_Ports_WriteByte(PIC2_DATA, m_secondaryPICMask);
	i686_PIC8259_EnableIRQ(2);
}

void i686_PIC8259_NotifyOnIRQTerm(uint8_t no) {
	if (no >= 8) {
		i686_Ports_WriteByte(PIC2_COMMAND, PIC_EOI);
	}
	i686_Ports_WriteByte(PIC1_COMMAND, PIC_EOI);
}

void i686_PIC8259_EnableIRQ(uint8_t no) {
	if (no >= 8) {
		no -= 8;
		m_secondaryPICMask &= ~(1 << no);
		i686_Ports_WriteByte(PIC2_DATA, m_secondaryPICMask);
	} else {
		m_primaryPICMask &= ~(1 << no);
		i686_Ports_WriteByte(PIC1_DATA, m_primaryPICMask);
	}
}

void i686_PIC8259_DisableIRQ(uint8_t no) {
	if (no >= 8) {
		no -= 8;
		m_secondaryPICMask |= (1 << no);
		i686_Ports_WriteByte(PIC2_DATA, m_secondaryPICMask);
	} else {
		m_primaryPICMask |= (1 << no);
		i686_Ports_WriteByte(PIC1_DATA, m_primaryPICMask);
	}
}

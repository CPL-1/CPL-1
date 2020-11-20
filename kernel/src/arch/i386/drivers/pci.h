#ifndef __I386_PCI_H_INCLUDED__
#define __I386_PCI_H_INCLUDED__

#include <common/misc/utils.h>

enum {
	I386_PCI_COMMAND = 0x04,
	I386_PCI_HEADER_TYPE = 0x0e,
	I386_PCI_VENDOR_ID = 0x00,
	I386_PCI_DEVICE_ID = 0x02,
	I386_PCI_NONE = 0xffff,
	I386_PCI_CLASS = 0x0b,
	I386_PCI_SUBCLASS = 0x0a,
	I386_PCI_TYPE_BRIDGE = 0x0604,
	I386_PCI_SECONDARY_BUS = 0x19,
	I386_PCI_BAR0 = 0x10,
	I386_PCI_BAR1 = 0x14,
	I386_PCI_BAR2 = 0x18,
	I386_PCI_BAR3 = 0x1C,
	I386_PCI_BAR4 = 0x20,
	I386_PCI_BAR5 = 0x24,
	I386_PCI_INT_LINE = 0x3C,
	I386_PCI_STATUS = 0x06,
};

struct i386_pci_id {
	uint16_t vendor_id;
	uint16_t device_id;
};

struct i386_pci_address {
	uint8_t bus;
	uint8_t slot;
	uint8_t function;
};

struct i386_pci_bar {
	uint32_t address;
	uint32_t size;

	bool is_mmio;
	bool disable_cache;
};

typedef void (*i386_pci_enumerator_t)(struct i386_pci_address addr,
									  struct i386_pci_id id, void *ctx);

void i386_pci_enable_bus_mastering(struct i386_pci_address address);
bool i386_pci_read_bar(struct i386_pci_address address, int index,
					   struct i386_pci_bar *bar);

uint8_t i386_pci_inb(struct i386_pci_address address, uint8_t field);
uint16_t i386_pci_inw(struct i386_pci_address address, uint8_t field);
uint32_t i386_pci_inl(struct i386_pci_address address, uint8_t field);

void i386_pci_outb(struct i386_pci_address address, uint8_t field,
				   uint8_t value);
void i386_pci_outw(struct i386_pci_address address, uint8_t field,
				   uint16_t value);
void i386_pci_outl(struct i386_pci_address address, uint8_t field,
				   uint32_t value);

void i386_pci_enumerate(i386_pci_enumerator_t enumerator, void *ctx);
uint16_t i386_pci_get_type(struct i386_pci_address address);

#endif
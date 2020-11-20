#ifndef __I686_PCI_H_INCLUDED__
#define __I686_PCI_H_INCLUDED__

#include <common/misc/utils.h>

enum {
	I686_PCI_COMMAND = 0x04,
	I686_PCI_HEADER_TYPE = 0x0e,
	I686_PCI_VENDOR_ID = 0x00,
	I686_PCI_DEVICE_ID = 0x02,
	I686_PCI_NONE = 0xffff,
	I686_PCI_CLASS = 0x0b,
	I686_PCI_SUBCLASS = 0x0a,
	I686_PCI_TYPE_BRIDGE = 0x0604,
	I686_PCI_SECONDARY_BUS = 0x19,
	I686_PCI_BAR0 = 0x10,
	I686_PCI_BAR1 = 0x14,
	I686_PCI_BAR2 = 0x18,
	I686_PCI_BAR3 = 0x1C,
	I686_PCI_BAR4 = 0x20,
	I686_PCI_BAR5 = 0x24,
	I686_PCI_INT_LINE = 0x3C,
	I686_PCI_STATUS = 0x06,
};

struct i686_pci_id {
	uint16_t vendor_id;
	uint16_t device_id;
};

struct i686_pci_address {
	uint8_t bus;
	uint8_t slot;
	uint8_t function;
};

struct i686_pci_bar {
	uint32_t address;
	uint32_t size;

	bool is_mmio;
	bool disable_cache;
};

typedef void (*i686_pci_enumerator_t)(struct i686_pci_address addr,
									  struct i686_pci_id id, void *ctx);

void i686_pci_enable_bus_mastering(struct i686_pci_address address);
bool i686_pci_read_bar(struct i686_pci_address address, int index,
					   struct i686_pci_bar *bar);

uint8_t i686_pci_inb(struct i686_pci_address address, uint8_t field);
uint16_t i686_pci_inw(struct i686_pci_address address, uint8_t field);
uint32_t i686_pci_inl(struct i686_pci_address address, uint8_t field);

void i686_pci_outb(struct i686_pci_address address, uint8_t field,
				   uint8_t value);
void i686_pci_outw(struct i686_pci_address address, uint8_t field,
				   uint16_t value);
void i686_pci_outl(struct i686_pci_address address, uint8_t field,
				   uint32_t value);

void i686_pci_enumerate(i686_pci_enumerator_t enumerator, void *ctx);
uint16_t i686_pci_get_type(struct i686_pci_address address);

#endif
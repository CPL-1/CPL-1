#ifndef __PCI_H_INCLUDED__
#define __PCI_H_INCLUDED__

#include <utils.h>

enum {
	PCI_COMMAND = 0x04,
	PCI_HEADER_TYPE = 0x0e,
	PCI_VENDOR_ID = 0x00,
	PCI_DEVICE_ID = 0x02,
	PCI_NONE = 0xffff,
	PCI_CLASS = 0x0b,
	PCI_SUBCLASS = 0x0a,
	PCI_TYPE_BRIDGE = 0x0604,
	PCI_SECONDARY_BUS = 0x19,
	PCI_BAR0 = 0x10,
	PCI_BAR1 = 0x14,
	PCI_BAR2 = 0x18,
	PCI_BAR3 = 0x1C,
	PCI_BAR4 = 0x20,
	PCI_BAR5 = 0x24,
	PCI_INT_LINE = 0x3C,
};

struct pci_id {
	uint16_t vendor_id;
	uint16_t device_id;
};

struct pci_address {
	uint8_t bus;
	uint8_t slot;
	uint8_t function;
};

struct pci_bar {
	uint32_t address;
	uint32_t size;

	bool is_mmio;
	bool disable_cache;
};

typedef void (*pci_enumerator_t)(struct pci_address addr, struct pci_id id,
                                 void *ctx);

void pci_enable_bus_mastering(struct pci_address address);
bool pci_read_bar(struct pci_address address, int index, struct pci_bar *bar);

uint8_t pci_inb(struct pci_address address, uint8_t field);
uint16_t pci_inw(struct pci_address address, uint8_t field);
uint32_t pci_inl(struct pci_address address, uint8_t field);

void pci_outb(struct pci_address address, uint8_t field, uint8_t value);
void pci_outw(struct pci_address address, uint8_t field, uint16_t value);
void pci_outl(struct pci_address address, uint8_t field, uint32_t value);

void pci_enumerate(pci_enumerator_t enumerator, void *ctx);
uint16_t pci_get_type(struct pci_address address);

#endif
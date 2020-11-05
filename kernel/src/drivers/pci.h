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

typedef void (*pci_enumerator_t)(struct pci_address addr, struct pci_id id,
                                 void *ctx);

void pci_enable_bus_master(struct pci_address address);

uint8_t pci_inb(struct pci_address address, uint8_t field);
uint16_t pci_inw(struct pci_address address, uint8_t field);
uint32_t pci_inl(struct pci_address address, uint8_t field);

void pci_outb(struct pci_address address, uint8_t field, uint8_t value);
void pci_outw(struct pci_address address, uint8_t field, uint16_t value);
void pci_outl(struct pci_address address, uint8_t field, uint32_t value);

void pci_enumerate(pci_enumerator_t enumerator, void *ctx);
uint16_t pci_get_type(struct pci_address address);

#endif
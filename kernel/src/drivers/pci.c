#include <drivers/pci.h>
#include <i386/ports.h>

inline static uint32_t pci_get_io_field_address(struct pci_address addr,
                                                uint8_t field) {
	return 0x80000000 | (addr.bus << 16) | (addr.slot << 11) |
	       (addr.function << 8) | (field & 0xfc);
}

#define PCI_ADDRESS_PORT 0xCF8
#define PCI_VALUE_PORT 0xCFC

uint8_t pci_inb(struct pci_address address, uint8_t field) {
	uint32_t io_address = pci_get_io_field_address(address, field);
	outl(PCI_ADDRESS_PORT, io_address);
	return inb(PCI_VALUE_PORT + (field & 3));
}

uint16_t pci_inw(struct pci_address address, uint8_t field) {
	uint32_t io_address = pci_get_io_field_address(address, field);
	outl(PCI_ADDRESS_PORT, io_address);
	return inw(PCI_VALUE_PORT + (field & 2));
}

uint32_t pci_inl(struct pci_address address, uint8_t field) {
	uint32_t io_address = pci_get_io_field_address(address, field);
	outl(PCI_ADDRESS_PORT, io_address);
	return inl(PCI_VALUE_PORT);
}

void pci_outb(struct pci_address address, uint8_t field, uint8_t value) {
	uint32_t io_address = pci_get_io_field_address(address, field);
	outl(PCI_ADDRESS_PORT, io_address);
	outb(PCI_VALUE_PORT, value + (field & 3));
}

void pci_outw(struct pci_address address, uint8_t field, uint16_t value) {
	uint32_t io_address = pci_get_io_field_address(address, field);
	outl(PCI_ADDRESS_PORT, io_address);
	outw(PCI_VALUE_PORT, value + (field & 2));
}

void pci_outl(struct pci_address address, uint8_t field, uint32_t value) {
	uint32_t io_address = pci_get_io_field_address(address, field);
	outl(PCI_ADDRESS_PORT, io_address);
	outl(PCI_VALUE_PORT, value);
}

void pci_enable_bus_master(struct pci_address address) {
	uint16_t command = pci_inw(address, PCI_COMMAND);
	command = command | (1 << 2) | (1 << 0);
	pci_outw(address, PCI_COMMAND, command);
}

static struct pci_address pci_make_default_address() {
	struct pci_address addr;
	addr.bus = 0;
	addr.slot = 0;
	addr.function = 0;
	return addr;
};

static struct pci_address pci_make_function_addr(uint8_t function) {
	struct pci_address addr = pci_make_default_address();
	addr.function = function;
	return addr;
}

static struct pci_address pci_make_bus_and_slot_addr(uint8_t bus,
                                                     uint8_t slot) {
	struct pci_address addr = pci_make_default_address();
	addr.bus = bus;
	addr.slot = slot;
	return addr;
}

uint16_t pci_get_type(struct pci_address address) {
	return ((uint16_t)(pci_inb(address, PCI_CLASS)) << 8) |
	       pci_inb(address, PCI_SUBCLASS);
}

static void pci_enumerate_bus(uint8_t bus, pci_enumerator_t enumerator,
                              void *ctx);

static void pci_enumerate_functions(uint8_t bus, uint8_t slot, uint8_t function,
                                    pci_enumerator_t enumerator, void *ctx) {
	struct pci_address addr;
	addr.bus = bus;
	addr.slot = slot;
	addr.function = function;
	struct pci_id id;
	id.vendor_id = pci_inw(addr, PCI_VENDOR_ID);
	id.device_id = pci_inw(addr, PCI_DEVICE_ID);
	enumerator(addr, id, ctx);
	uint16_t type = pci_get_type(addr);
	if (type == PCI_TYPE_BRIDGE) {
		uint8_t secondary_bus = pci_inb(addr, PCI_SECONDARY_BUS);
		pci_enumerate_bus(secondary_bus, enumerator, ctx);
	}
}

static void pci_enumerate_slot(uint8_t bus, uint8_t slot,
                               pci_enumerator_t enumerator, void *ctx) {
	struct pci_address slot_address = pci_make_bus_and_slot_addr(bus, slot);
	if (pci_inw(slot_address, PCI_VENDOR_ID) == PCI_NONE) {
		return;
	}
	if ((pci_inb(slot_address, PCI_HEADER_TYPE) & 0x80) == 0) {
		pci_enumerate_functions(bus, slot, 0, enumerator, ctx);
	} else {
		for (uint8_t i = 0; i < 8; ++i) {
			struct pci_address addr;
			addr.bus = bus;
			addr.slot = slot;
			addr.function = i;
			if (pci_inw(addr, PCI_VENDOR_ID) != PCI_NONE) {
				pci_enumerate_functions(bus, slot, i, enumerator, ctx);
			}
		}
	}
}

static void pci_enumerate_bus(uint8_t bus, pci_enumerator_t enumerator,
                              void *ctx) {
	for (uint8_t slot = 0; slot < 32; ++slot) {
		pci_enumerate_slot(bus, slot, enumerator, ctx);
	}
}

void pci_enumerate(pci_enumerator_t enumerator, void *ctx) {
	if ((pci_inb(pci_make_default_address(), PCI_HEADER_TYPE) & 0x80) == 0) {
		pci_enumerate_bus(0, enumerator, ctx);
	} else {
		for (uint8_t i = 0; i < 8; ++i) {
			if (pci_inw(pci_make_function_addr(i), PCI_VENDOR_ID) == PCI_NONE) {
				break;
			}
			pci_enumerate_bus(i, enumerator, ctx);
		}
	}
}
#include <drivers/pci.h>
#include <i386/ports.h>

inline static uint32_t pci_get_io_field_address(struct pci_address addr,
                                                uint8_t field) {
	return 0x80000000 | (addr.bus << 16) | (addr.slot << 11) |
	       (addr.function << 8) | (field & ~((uint32_t)(3)));
}

#define PCI_ADDRESS_PORT 0xcf8
#define PCI_VALUE_PORT 0xcfc

uint8_t pci_inb(struct pci_address address, uint8_t field) {
	uint32_t io_address = pci_get_io_field_address(address, field);
	outl(PCI_ADDRESS_PORT, io_address);
	return inb(PCI_VALUE_PORT + (field & 3));
}

uint16_t pci_inw(struct pci_address address, uint8_t field) {
	uint32_t io_address = pci_get_io_field_address(address, field);
	outl(PCI_ADDRESS_PORT, io_address);
	return inw(PCI_VALUE_PORT + (field & 3));
}

uint32_t pci_inl(struct pci_address address, uint8_t field) {
	uint32_t io_address = pci_get_io_field_address(address, field);
	outl(PCI_ADDRESS_PORT, io_address);
	return inl(PCI_VALUE_PORT + (field & 3));
}

void pci_outb(struct pci_address address, uint8_t field, uint8_t value) {
	uint32_t io_address = pci_get_io_field_address(address, field);
	outl(PCI_ADDRESS_PORT, io_address);
	outb(PCI_VALUE_PORT + (field & 3), value);
}

void pci_outw(struct pci_address address, uint8_t field, uint16_t value) {
	uint32_t io_address = pci_get_io_field_address(address, field);
	outl(PCI_ADDRESS_PORT, io_address);
	outw(PCI_VALUE_PORT + (field & 3), value);
}

void pci_outl(struct pci_address address, uint8_t field, uint32_t value) {
	uint32_t io_address = pci_get_io_field_address(address, field);
	outl(PCI_ADDRESS_PORT, io_address);
	outl(PCI_VALUE_PORT + (field & 3), value);
}

void pci_enable_bus_mastering(struct pci_address address) {
	uint16_t command = pci_inl(address, PCI_COMMAND);
	command = command | (1 << 2) | (1 << 0);
	pci_outl(address, PCI_COMMAND, command);
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

bool pci_read_bar(struct pci_address address, int index, struct pci_bar *bar) {
	uint8_t reg = 4 * index + 0x10;
	uint32_t bar_orig_address = pci_inl(address, reg);
	uint32_t bar_address = bar_orig_address;
	if (bar_address == 0) {
		return false;
	}
	bar->is_mmio = ((bar_address & 1) == 0);
	bar->disable_cache = false;
	if (bar->is_mmio && ((bar_address & (1 << 3)) != 0)) {
		bar->disable_cache = true;
	}
	bool is_64bit =
	    (bar->is_mmio && ((((bar_address >> 1U) & 0b11U)) == 0b10U));
	if (is_64bit) {
		uint32_t bar_high_address = pci_inl(address, reg + 4);
		if (bar_high_address != 0) {
			return false;
		}
	}
	bar_address &= ~(bar->is_mmio ? (0b1111U) : (0b11U));
	pci_outl(address, reg, 0xffffffff);
	uint32_t bar_size_low = pci_inl(address, reg);
	uint32_t bar_size_high = 0;
	pci_outl(address, reg, bar_orig_address);
	if (is_64bit) {
		pci_outl(address, reg + 4, 0xffffffff);
		bar_size_high = pci_inl(address, reg + 4);
		pci_outl(address, reg + 4, 0);
	}
	uint64_t bar_size =
	    (((uint64_t)bar_size_high) << 32ULL) | ((uint64_t)bar_size_low);
	bar_size &= ~(bar->is_mmio ? (0b1111ULL) : (0b11ULL));
	bar_size = ~bar_size + 1ULL;
	if (bar_size > 0xffffffff) {
		return false;
	}
	bar->size = (uint32_t)bar_size;
	bar->address = bar_address;
	return true;
}
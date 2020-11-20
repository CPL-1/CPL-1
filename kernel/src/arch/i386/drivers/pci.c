#include <arch/i386/cpu/ports.h>
#include <arch/i386/drivers/pci.h>

inline static uint32_t
i386_pci_get_io_field_address(struct i386_pci_address addr, uint8_t field) {
	return 0x80000000 | (addr.bus << 16) | (addr.slot << 11) |
		   (addr.function << 8) | (field & ~((uint32_t)(3)));
}

#define I386_PCI_ADDRESS_PORT 0xcf8
#define I386_PCI_VALUE_PORT 0xcfc

uint8_t i386_pci_inb(struct i386_pci_address address, uint8_t field) {
	uint32_t io_address = i386_pci_get_io_field_address(address, field);
	outl(I386_PCI_ADDRESS_PORT, io_address);
	return inb(I386_PCI_VALUE_PORT + (field & 3));
}

uint16_t i386_pci_inw(struct i386_pci_address address, uint8_t field) {
	uint32_t io_address = i386_pci_get_io_field_address(address, field);
	outl(I386_PCI_ADDRESS_PORT, io_address);
	return inw(I386_PCI_VALUE_PORT + (field & 3));
}

uint32_t i386_pci_inl(struct i386_pci_address address, uint8_t field) {
	uint32_t io_address = i386_pci_get_io_field_address(address, field);
	outl(I386_PCI_ADDRESS_PORT, io_address);
	return inl(I386_PCI_VALUE_PORT + (field & 3));
}

void i386_pci_outb(struct i386_pci_address address, uint8_t field,
				   uint8_t value) {
	uint32_t io_address = i386_pci_get_io_field_address(address, field);
	outl(I386_PCI_ADDRESS_PORT, io_address);
	outb(I386_PCI_VALUE_PORT + (field & 3), value);
}

void i386_pci_outw(struct i386_pci_address address, uint8_t field,
				   uint16_t value) {
	uint32_t io_address = i386_pci_get_io_field_address(address, field);
	outl(I386_PCI_ADDRESS_PORT, io_address);
	outw(I386_PCI_VALUE_PORT + (field & 3), value);
}

void i386_pci_outl(struct i386_pci_address address, uint8_t field,
				   uint32_t value) {
	uint32_t io_address = i386_pci_get_io_field_address(address, field);
	outl(I386_PCI_ADDRESS_PORT, io_address);
	outl(I386_PCI_VALUE_PORT + (field & 3), value);
}

void i386_pci_enable_bus_mastering(struct i386_pci_address address) {
	uint16_t command = i386_pci_inl(address, I386_PCI_COMMAND);
	command = command | (1 << 2) | (1 << 0);
	i386_pci_outl(address, I386_PCI_COMMAND, command);
}

static struct i386_pci_address i386_pci_make_default_address() {
	struct i386_pci_address addr;
	addr.bus = 0;
	addr.slot = 0;
	addr.function = 0;
	return addr;
};

static struct i386_pci_address i386_pci_make_function_addr(uint8_t function) {
	struct i386_pci_address addr = i386_pci_make_default_address();
	addr.function = function;
	return addr;
}

static struct i386_pci_address pci_make_bus_and_slot_addr(uint8_t bus,
														  uint8_t slot) {
	struct i386_pci_address addr = i386_pci_make_default_address();
	addr.bus = bus;
	addr.slot = slot;
	return addr;
}

uint16_t i386_pci_get_type(struct i386_pci_address address) {
	return ((uint16_t)(i386_pci_inb(address, I386_PCI_CLASS)) << 8) |
		   i386_pci_inb(address, I386_PCI_SUBCLASS);
}

static void i386_pci_enumerate_bus(uint8_t bus,
								   i386_pci_enumerator_t enumerator, void *ctx);

static void i386_pci_enumerate_functions(uint8_t bus, uint8_t slot,
										 uint8_t function,
										 i386_pci_enumerator_t enumerator,
										 void *ctx) {
	struct i386_pci_address addr;
	addr.bus = bus;
	addr.slot = slot;
	addr.function = function;
	struct i386_pci_id id;
	id.vendor_id = i386_pci_inw(addr, I386_PCI_VENDOR_ID);
	id.device_id = i386_pci_inw(addr, I386_PCI_DEVICE_ID);
	enumerator(addr, id, ctx);
	uint16_t type = i386_pci_get_type(addr);
	if (type == I386_PCI_TYPE_BRIDGE) {
		uint8_t secondary_bus = i386_pci_inb(addr, I386_PCI_SECONDARY_BUS);
		i386_pci_enumerate_bus(secondary_bus, enumerator, ctx);
	}
}

static void i386_pci_enumerate_slot(uint8_t bus, uint8_t slot,
									i386_pci_enumerator_t enumerator,
									void *ctx) {
	struct i386_pci_address slot_address =
		pci_make_bus_and_slot_addr(bus, slot);
	if (i386_pci_inw(slot_address, I386_PCI_VENDOR_ID) == I386_PCI_NONE) {
		return;
	}
	if ((i386_pci_inb(slot_address, I386_PCI_HEADER_TYPE) & 0x80) == 0) {
		i386_pci_enumerate_functions(bus, slot, 0, enumerator, ctx);
	} else {
		for (uint8_t i = 0; i < 8; ++i) {
			struct i386_pci_address addr;
			addr.bus = bus;
			addr.slot = slot;
			addr.function = i;
			if (i386_pci_inw(addr, I386_PCI_VENDOR_ID) != I386_PCI_NONE) {
				i386_pci_enumerate_functions(bus, slot, i, enumerator, ctx);
			}
		}
	}
}

static void i386_pci_enumerate_bus(uint8_t bus,
								   i386_pci_enumerator_t enumerator,
								   void *ctx) {
	for (uint8_t slot = 0; slot < 32; ++slot) {
		i386_pci_enumerate_slot(bus, slot, enumerator, ctx);
	}
}

void i386_pci_enumerate(i386_pci_enumerator_t enumerator, void *ctx) {
	if ((i386_pci_inb(i386_pci_make_default_address(), I386_PCI_HEADER_TYPE) &
		 0x80) == 0) {
		i386_pci_enumerate_bus(0, enumerator, ctx);
	} else {
		for (uint8_t i = 0; i < 8; ++i) {
			if (i386_pci_inw(i386_pci_make_function_addr(i),
							 I386_PCI_VENDOR_ID) == I386_PCI_NONE) {
				break;
			}
			i386_pci_enumerate_bus(i, enumerator, ctx);
		}
	}
}

bool i386_pci_read_bar(struct i386_pci_address address, int index,
					   struct i386_pci_bar *bar) {
	uint8_t reg = 4 * index + 0x10;
	uint32_t bar_orig_address = i386_pci_inl(address, reg);
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
		uint32_t bar_high_address = i386_pci_inl(address, reg + 4);
		if (bar_high_address != 0) {
			return false;
		}
	}
	bar_address &= ~(bar->is_mmio ? (0b1111U) : (0b11U));
	i386_pci_outl(address, reg, 0xffffffff);
	uint32_t bar_size_low = i386_pci_inl(address, reg);
	uint32_t bar_size_high = 0;
	i386_pci_outl(address, reg, bar_orig_address);
	if (is_64bit) {
		i386_pci_outl(address, reg + 4, 0xffffffff);
		bar_size_high = i386_pci_inl(address, reg + 4);
		i386_pci_outl(address, reg + 4, 0);
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
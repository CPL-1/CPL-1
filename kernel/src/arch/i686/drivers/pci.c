#include <arch/i686/cpu/ports.h>
#include <arch/i686/drivers/pci.h>

inline static uint32_t
i686_pci_get_io_field_address(struct i686_pci_address addr, uint8_t field) {
	return 0x80000000 | (addr.bus << 16) | (addr.slot << 11) |
		   (addr.function << 8) | (field & ~((uint32_t)(3)));
}

#define I686_PCI_ADDRESS_PORT 0xcf8
#define I686_PCI_VALUE_PORT 0xcfc

uint8_t i686_pci_inb(struct i686_pci_address address, uint8_t field) {
	uint32_t io_address = i686_pci_get_io_field_address(address, field);
	outl(I686_PCI_ADDRESS_PORT, io_address);
	return inb(I686_PCI_VALUE_PORT + (field & 3));
}

uint16_t i686_pci_inw(struct i686_pci_address address, uint8_t field) {
	uint32_t io_address = i686_pci_get_io_field_address(address, field);
	outl(I686_PCI_ADDRESS_PORT, io_address);
	return inw(I686_PCI_VALUE_PORT + (field & 3));
}

uint32_t i686_pci_inl(struct i686_pci_address address, uint8_t field) {
	uint32_t io_address = i686_pci_get_io_field_address(address, field);
	outl(I686_PCI_ADDRESS_PORT, io_address);
	return inl(I686_PCI_VALUE_PORT + (field & 3));
}

void i686_pci_outb(struct i686_pci_address address, uint8_t field,
				   uint8_t value) {
	uint32_t io_address = i686_pci_get_io_field_address(address, field);
	outl(I686_PCI_ADDRESS_PORT, io_address);
	outb(I686_PCI_VALUE_PORT + (field & 3), value);
}

void i686_pci_outw(struct i686_pci_address address, uint8_t field,
				   uint16_t value) {
	uint32_t io_address = i686_pci_get_io_field_address(address, field);
	outl(I686_PCI_ADDRESS_PORT, io_address);
	outw(I686_PCI_VALUE_PORT + (field & 3), value);
}

void i686_pci_outl(struct i686_pci_address address, uint8_t field,
				   uint32_t value) {
	uint32_t io_address = i686_pci_get_io_field_address(address, field);
	outl(I686_PCI_ADDRESS_PORT, io_address);
	outl(I686_PCI_VALUE_PORT + (field & 3), value);
}

void i686_pci_enable_bus_mastering(struct i686_pci_address address) {
	uint16_t command = i686_pci_inl(address, I686_PCI_COMMAND);
	command = command | (1 << 2) | (1 << 0);
	i686_pci_outl(address, I686_PCI_COMMAND, command);
}

static struct i686_pci_address i686_pci_make_default_address() {
	struct i686_pci_address addr;
	addr.bus = 0;
	addr.slot = 0;
	addr.function = 0;
	return addr;
};

static struct i686_pci_address i686_pci_make_function_addr(uint8_t function) {
	struct i686_pci_address addr = i686_pci_make_default_address();
	addr.function = function;
	return addr;
}

static struct i686_pci_address pci_make_bus_and_slot_addr(uint8_t bus,
														  uint8_t slot) {
	struct i686_pci_address addr = i686_pci_make_default_address();
	addr.bus = bus;
	addr.slot = slot;
	return addr;
}

uint16_t i686_pci_get_type(struct i686_pci_address address) {
	return ((uint16_t)(i686_pci_inb(address, I686_PCI_CLASS)) << 8) |
		   i686_pci_inb(address, I686_PCI_SUBCLASS);
}

static void i686_pci_enumerate_bus(uint8_t bus,
								   i686_pci_enumerator_t enumerator, void *ctx);

static void i686_pci_enumerate_functions(uint8_t bus, uint8_t slot,
										 uint8_t function,
										 i686_pci_enumerator_t enumerator,
										 void *ctx) {
	struct i686_pci_address addr;
	addr.bus = bus;
	addr.slot = slot;
	addr.function = function;
	struct i686_pci_id id;
	id.vendor_id = i686_pci_inw(addr, I686_PCI_VENDOR_ID);
	id.device_id = i686_pci_inw(addr, I686_PCI_DEVICE_ID);
	enumerator(addr, id, ctx);
	uint16_t type = i686_pci_get_type(addr);
	if (type == I686_PCI_TYPE_BRIDGE) {
		uint8_t secondary_bus = i686_pci_inb(addr, I686_PCI_SECONDARY_BUS);
		i686_pci_enumerate_bus(secondary_bus, enumerator, ctx);
	}
}

static void i686_pci_enumerate_slot(uint8_t bus, uint8_t slot,
									i686_pci_enumerator_t enumerator,
									void *ctx) {
	struct i686_pci_address slot_address =
		pci_make_bus_and_slot_addr(bus, slot);
	if (i686_pci_inw(slot_address, I686_PCI_VENDOR_ID) == I686_PCI_NONE) {
		return;
	}
	if ((i686_pci_inb(slot_address, I686_PCI_HEADER_TYPE) & 0x80) == 0) {
		i686_pci_enumerate_functions(bus, slot, 0, enumerator, ctx);
	} else {
		for (uint8_t i = 0; i < 8; ++i) {
			struct i686_pci_address addr;
			addr.bus = bus;
			addr.slot = slot;
			addr.function = i;
			if (i686_pci_inw(addr, I686_PCI_VENDOR_ID) != I686_PCI_NONE) {
				i686_pci_enumerate_functions(bus, slot, i, enumerator, ctx);
			}
		}
	}
}

static void i686_pci_enumerate_bus(uint8_t bus,
								   i686_pci_enumerator_t enumerator,
								   void *ctx) {
	for (uint8_t slot = 0; slot < 32; ++slot) {
		i686_pci_enumerate_slot(bus, slot, enumerator, ctx);
	}
}

void i686_pci_enumerate(i686_pci_enumerator_t enumerator, void *ctx) {
	if ((i686_pci_inb(i686_pci_make_default_address(), I686_PCI_HEADER_TYPE) &
		 0x80) == 0) {
		i686_pci_enumerate_bus(0, enumerator, ctx);
	} else {
		for (uint8_t i = 0; i < 8; ++i) {
			if (i686_pci_inw(i686_pci_make_function_addr(i),
							 I686_PCI_VENDOR_ID) == I686_PCI_NONE) {
				break;
			}
			i686_pci_enumerate_bus(i, enumerator, ctx);
		}
	}
}

bool i686_pci_read_bar(struct i686_pci_address address, int index,
					   struct i686_pci_bar *bar) {
	uint8_t reg = 4 * index + 0x10;
	uint32_t bar_orig_address = i686_pci_inl(address, reg);
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
		uint32_t bar_high_address = i686_pci_inl(address, reg + 4);
		if (bar_high_address != 0) {
			return false;
		}
	}
	bar_address &= ~(bar->is_mmio ? (0b1111U) : (0b11U));
	i686_pci_outl(address, reg, 0xffffffff);
	uint32_t bar_size_low = i686_pci_inl(address, reg);
	uint32_t bar_size_high = 0;
	i686_pci_outl(address, reg, bar_orig_address);
	if (is_64bit) {
		i686_pci_outl(address, reg + 4, 0xffffffff);
		bar_size_high = i686_pci_inl(address, reg + 4);
		i686_pci_outl(address, reg + 4, 0);
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
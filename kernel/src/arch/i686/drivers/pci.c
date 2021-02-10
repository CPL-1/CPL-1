#include <arch/i686/cpu/ports.h>
#include <arch/i686/drivers/pci.h>

INLINE static uint32_t i686_pci_get_io_field_address(struct i686_PCI_Address addr, uint8_t field) {
	return 0x80000000 | (addr.bus << 16) | (addr.slot << 11) | (addr.function << 8) | (field & ~((uint32_t)(3)));
}

#define I686_PCI_ADDRESS_PORT 0xcf8
#define I686_PCI_VALUE_PORT 0xcfc

uint8_t i686_PCI_ReadByte(struct i686_PCI_Address address, uint8_t field) {
	uint32_t ioAddress = i686_pci_get_io_field_address(address, field);
	i686_Ports_WriteDoubleWord(I686_PCI_ADDRESS_PORT, ioAddress);
	return i686_Ports_ReadByte(I686_PCI_VALUE_PORT + (field & 3));
}

uint16_t i686_PCI_ReadWord(struct i686_PCI_Address address, uint8_t field) {
	uint32_t ioAddress = i686_pci_get_io_field_address(address, field);
	i686_Ports_WriteDoubleWord(I686_PCI_ADDRESS_PORT, ioAddress);
	return i686_Ports_ReadWord(I686_PCI_VALUE_PORT + (field & 3));
}

uint32_t i686_PCI_ReadDoubleWord(struct i686_PCI_Address address, uint8_t field) {
	uint32_t ioAddress = i686_pci_get_io_field_address(address, field);
	i686_Ports_WriteDoubleWord(I686_PCI_ADDRESS_PORT, ioAddress);
	return i686_Ports_ReadDoubleWord(I686_PCI_VALUE_PORT + (field & 3));
}

void i686_PCI_WriteByte(struct i686_PCI_Address address, uint8_t field, uint8_t value) {
	uint32_t ioAddress = i686_pci_get_io_field_address(address, field);
	i686_Ports_WriteDoubleWord(I686_PCI_ADDRESS_PORT, ioAddress);
	i686_Ports_WriteByte(I686_PCI_VALUE_PORT + (field & 3), value);
}

void i686_PCI_WriteWord(struct i686_PCI_Address address, uint8_t field, uint16_t value) {
	uint32_t ioAddress = i686_pci_get_io_field_address(address, field);
	i686_Ports_WriteDoubleWord(I686_PCI_ADDRESS_PORT, ioAddress);
	i686_Ports_WriteWord(I686_PCI_VALUE_PORT + (field & 3), value);
}

void i686_PCI_WriteDoubleWord(struct i686_PCI_Address address, uint8_t field, uint32_t value) {
	uint32_t ioAddress = i686_pci_get_io_field_address(address, field);
	i686_Ports_WriteDoubleWord(I686_PCI_ADDRESS_PORT, ioAddress);
	i686_Ports_WriteDoubleWord(I686_PCI_VALUE_PORT + (field & 3), value);
}

void i686_PCI_EnableBusMastering(struct i686_PCI_Address address) {
	uint16_t command = i686_PCI_ReadDoubleWord(address, I686_PCI_COMMAND);
	command = command | (1 << 2) | (1 << 0);
	i686_PCI_WriteDoubleWord(address, I686_PCI_COMMAND, command);
}

static struct i686_PCI_Address i686_PCI_MakeDefaultAddress() {
	struct i686_PCI_Address addr;
	addr.bus = 0;
	addr.slot = 0;
	addr.function = 0;
	return addr;
};

static struct i686_PCI_Address i686_PCI_MakeAddressWithFunction(uint8_t function) {
	struct i686_PCI_Address addr = i686_PCI_MakeDefaultAddress();
	addr.function = function;
	return addr;
}

static struct i686_PCI_Address i686_PCI_MakeAddressWithBusAndSlot(uint8_t bus, uint8_t slot) {
	struct i686_PCI_Address addr = i686_PCI_MakeDefaultAddress();
	addr.bus = bus;
	addr.slot = slot;
	return addr;
}

uint16_t i686_PCI_GetDeviceType(struct i686_PCI_Address address) {
	return ((uint16_t)(i686_PCI_ReadByte(address, I686_PCI_CLASS)) << 8) |
		   i686_PCI_ReadByte(address, I686_PCI_SUBCLASS);
}

static void i686_PCI_EnumerateBus(uint8_t bus, i686_pci_enumerator_t enumerator, void *ctx);

static void i686_PCI_EnumerateFunctions(uint8_t bus, uint8_t slot, uint8_t function, i686_pci_enumerator_t enumerator,
										void *ctx) {
	struct i686_PCI_Address addr;
	addr.bus = bus;
	addr.slot = slot;
	addr.function = function;
	struct i686_PCI_ID id;
	id.vendor_id = i686_PCI_ReadWord(addr, I686_PCI_VENDOR_ID);
	id.device_id = i686_PCI_ReadWord(addr, I686_PCI_DEVICE_ID);
	enumerator(addr, id, ctx);
	uint16_t type = i686_PCI_GetDeviceType(addr);
	if (type == I686_PCI_TYPE_BRIDGE) {
		uint8_t secondary_bus = i686_PCI_ReadByte(addr, I686_PCI_SECONDARY_BUS);
		i686_PCI_EnumerateBus(secondary_bus, enumerator, ctx);
	}
}

static void i686_PCI_EnumerateSlot(uint8_t bus, uint8_t slot, i686_pci_enumerator_t enumerator, void *ctx) {
	struct i686_PCI_Address slotAddress = i686_PCI_MakeAddressWithBusAndSlot(bus, slot);
	if (i686_PCI_ReadWord(slotAddress, I686_PCI_VENDOR_ID) == I686_PCI_NONE) {
		return;
	}
	if ((i686_PCI_ReadByte(slotAddress, I686_PCI_HEADER_TYPE) & 0x80) == 0) {
		i686_PCI_EnumerateFunctions(bus, slot, 0, enumerator, ctx);
	} else {
		for (uint8_t i = 0; i < 8; ++i) {
			struct i686_PCI_Address addr;
			addr.bus = bus;
			addr.slot = slot;
			addr.function = i;
			if (i686_PCI_ReadWord(addr, I686_PCI_VENDOR_ID) != I686_PCI_NONE) {
				i686_PCI_EnumerateFunctions(bus, slot, i, enumerator, ctx);
			}
		}
	}
}

static void i686_PCI_EnumerateBus(uint8_t bus, i686_pci_enumerator_t enumerator, void *ctx) {
	for (uint8_t slot = 0; slot < 32; ++slot) {
		i686_PCI_EnumerateSlot(bus, slot, enumerator, ctx);
	}
}

void i686_PCI_Enumerate(i686_pci_enumerator_t enumerator, void *ctx) {
	if ((i686_PCI_ReadByte(i686_PCI_MakeDefaultAddress(), I686_PCI_HEADER_TYPE) & 0x80) == 0) {
		i686_PCI_EnumerateBus(0, enumerator, ctx);
	} else {
		for (uint8_t i = 0; i < 8; ++i) {
			if (i686_PCI_ReadWord(i686_PCI_MakeAddressWithFunction(i), I686_PCI_VENDOR_ID) == I686_PCI_NONE) {
				break;
			}
			i686_PCI_EnumerateBus(i, enumerator, ctx);
		}
	}
}

bool i686_PCI_ReadBAR(struct i686_PCI_Address address, int index, struct i686_pci_bar *bar) {
	uint8_t reg = 4 * index + 0x10;
	uint32_t barOrigAddress = i686_PCI_ReadDoubleWord(address, reg);
	uint32_t barAddress = barOrigAddress;
	if (barAddress == 0) {
		return false;
	}
	bar->isMMIO = ((barAddress & 1) == 0);
	bar->disableCache = false;
	if (bar->isMMIO && ((barAddress & (1 << 3)) != 0)) {
		bar->disableCache = true;
	}
	bool is64Bit = (bar->isMMIO && ((((barAddress >> 1U) & 0b11U)) == 0b10U));
	if (is64Bit) {
		uint32_t bar_high_address = i686_PCI_ReadDoubleWord(address, reg + 4);
		if (bar_high_address != 0) {
			return false;
		}
	}
	barAddress &= ~(bar->isMMIO ? (0b1111U) : (0b11U));
	i686_PCI_WriteDoubleWord(address, reg, 0xffffffff);
	uint32_t barSizeLow = i686_PCI_ReadDoubleWord(address, reg);
	uint32_t barSizeHigh = 0;
	i686_PCI_WriteDoubleWord(address, reg, barOrigAddress);
	if (is64Bit) {
		i686_PCI_WriteDoubleWord(address, reg + 4, 0xffffffff);
		barSizeHigh = i686_PCI_ReadDoubleWord(address, reg + 4);
		i686_PCI_WriteDoubleWord(address, reg + 4, 0);
	}
	uint64_t barSize = (((uint64_t)barSizeHigh) << 32ULL) | ((uint64_t)barSizeLow);
	barSize &= ~(bar->isMMIO ? (0b1111ULL) : (0b11ULL));
	barSize = ~barSize + 1ULL;
	if (barSize > 0xffffffff) {
		return false;
	}
	bar->size = (uint32_t)barSize;
	bar->address = barAddress;
	return true;
}

#include <arch/i686/cpu/ports.h>
#include <arch/i686/drivers/pci.h>

INLINE static UINT32 i686_pci_get_io_field_address(struct i686_PCI_Address addr, UINT8 field) {
	return 0x80000000 | (addr.bus << 16) | (addr.slot << 11) | (addr.function << 8) | (field & ~((UINT32)(3)));
}

#define I686_PCI_ADDRESS_PORT 0xcf8
#define I686_PCI_VALUE_PORT 0xcfc

UINT8 i686_PCI_ReadByte(struct i686_PCI_Address address, UINT8 field) {
	UINT32 ioAddress = i686_pci_get_io_field_address(address, field);
	i686_Ports_WriteDoubleWord(I686_PCI_ADDRESS_PORT, ioAddress);
	return i686_Ports_ReadByte(I686_PCI_VALUE_PORT + (field & 3));
}

UINT16 i686_PCI_ReadWord(struct i686_PCI_Address address, UINT8 field) {
	UINT32 ioAddress = i686_pci_get_io_field_address(address, field);
	i686_Ports_WriteDoubleWord(I686_PCI_ADDRESS_PORT, ioAddress);
	return i686_Ports_ReadWord(I686_PCI_VALUE_PORT + (field & 3));
}

UINT32 i686_PCI_ReadDoubleWord(struct i686_PCI_Address address, UINT8 field) {
	UINT32 ioAddress = i686_pci_get_io_field_address(address, field);
	i686_Ports_WriteDoubleWord(I686_PCI_ADDRESS_PORT, ioAddress);
	return i686_Ports_ReadDoubleWord(I686_PCI_VALUE_PORT + (field & 3));
}

void i686_PCI_WriteByte(struct i686_PCI_Address address, UINT8 field, UINT8 value) {
	UINT32 ioAddress = i686_pci_get_io_field_address(address, field);
	i686_Ports_WriteDoubleWord(I686_PCI_ADDRESS_PORT, ioAddress);
	i686_Ports_WriteByte(I686_PCI_VALUE_PORT + (field & 3), value);
}

void i686_PCI_WriteWord(struct i686_PCI_Address address, UINT8 field, UINT16 value) {
	UINT32 ioAddress = i686_pci_get_io_field_address(address, field);
	i686_Ports_WriteDoubleWord(I686_PCI_ADDRESS_PORT, ioAddress);
	i686_Ports_WriteWord(I686_PCI_VALUE_PORT + (field & 3), value);
}

void i686_PCI_WriteDoubleWord(struct i686_PCI_Address address, UINT8 field, UINT32 value) {
	UINT32 ioAddress = i686_pci_get_io_field_address(address, field);
	i686_Ports_WriteDoubleWord(I686_PCI_ADDRESS_PORT, ioAddress);
	i686_Ports_WriteDoubleWord(I686_PCI_VALUE_PORT + (field & 3), value);
}

void i686_PCI_EnableBusMastering(struct i686_PCI_Address address) {
	UINT16 command = i686_PCI_ReadDoubleWord(address, I686_PCI_COMMAND);
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

static struct i686_PCI_Address i686_PCI_MakeAddressWithFunction(UINT8 function) {
	struct i686_PCI_Address addr = i686_PCI_MakeDefaultAddress();
	addr.function = function;
	return addr;
}

static struct i686_PCI_Address i686_PCI_MakeAddressWithBusAndSlot(UINT8 bus, UINT8 slot) {
	struct i686_PCI_Address addr = i686_PCI_MakeDefaultAddress();
	addr.bus = bus;
	addr.slot = slot;
	return addr;
}

UINT16 i686_PCI_GetDeviceType(struct i686_PCI_Address address) {
	return ((UINT16)(i686_PCI_ReadByte(address, I686_PCI_CLASS)) << 8) | i686_PCI_ReadByte(address, I686_PCI_SUBCLASS);
}

static void i686_PCI_EnumerateBus(UINT8 bus, i686_pci_enumerator_t enumerator, void *ctx);

static void i686_PCI_EnumerateFunctions(UINT8 bus, UINT8 slot, UINT8 function, i686_pci_enumerator_t enumerator,
										void *ctx) {
	struct i686_PCI_Address addr;
	addr.bus = bus;
	addr.slot = slot;
	addr.function = function;
	struct i686_PCI_ID id;
	id.vendor_id = i686_PCI_ReadWord(addr, I686_PCI_VENDOR_ID);
	id.device_id = i686_PCI_ReadWord(addr, I686_PCI_DEVICE_ID);
	enumerator(addr, id, ctx);
	UINT16 type = i686_PCI_GetDeviceType(addr);
	if (type == I686_PCI_TYPE_BRIDGE) {
		UINT8 secondary_bus = i686_PCI_ReadByte(addr, I686_PCI_SECONDARY_BUS);
		i686_PCI_EnumerateBus(secondary_bus, enumerator, ctx);
	}
}

static void i686_PCI_EnumerateSlot(UINT8 bus, UINT8 slot, i686_pci_enumerator_t enumerator, void *ctx) {
	struct i686_PCI_Address slotAddress = i686_PCI_MakeAddressWithBusAndSlot(bus, slot);
	if (i686_PCI_ReadWord(slotAddress, I686_PCI_VENDOR_ID) == I686_PCI_NONE) {
		return;
	}
	if ((i686_PCI_ReadByte(slotAddress, I686_PCI_HEADER_TYPE) & 0x80) == 0) {
		i686_PCI_EnumerateFunctions(bus, slot, 0, enumerator, ctx);
	} else {
		for (UINT8 i = 0; i < 8; ++i) {
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

static void i686_PCI_EnumerateBus(UINT8 bus, i686_pci_enumerator_t enumerator, void *ctx) {
	for (UINT8 slot = 0; slot < 32; ++slot) {
		i686_PCI_EnumerateSlot(bus, slot, enumerator, ctx);
	}
}

void i686_PCI_Enumerate(i686_pci_enumerator_t enumerator, void *ctx) {
	if ((i686_PCI_ReadByte(i686_PCI_MakeDefaultAddress(), I686_PCI_HEADER_TYPE) & 0x80) == 0) {
		i686_PCI_EnumerateBus(0, enumerator, ctx);
	} else {
		for (UINT8 i = 0; i < 8; ++i) {
			if (i686_PCI_ReadWord(i686_PCI_MakeAddressWithFunction(i), I686_PCI_VENDOR_ID) == I686_PCI_NONE) {
				break;
			}
			i686_PCI_EnumerateBus(i, enumerator, ctx);
		}
	}
}

bool i686_PCI_ReadBAR(struct i686_PCI_Address address, int index, struct i686_pci_bar *bar) {
	UINT8 reg = 4 * index + 0x10;
	UINT32 barOrigAddress = i686_PCI_ReadDoubleWord(address, reg);
	UINT32 barAddress = barOrigAddress;
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
		UINT32 bar_high_address = i686_PCI_ReadDoubleWord(address, reg + 4);
		if (bar_high_address != 0) {
			return false;
		}
	}
	barAddress &= ~(bar->isMMIO ? (0b1111U) : (0b11U));
	i686_PCI_WriteDoubleWord(address, reg, 0xffffffff);
	UINT32 barSizeLow = i686_PCI_ReadDoubleWord(address, reg);
	UINT32 barSizeHigh = 0;
	i686_PCI_WriteDoubleWord(address, reg, barOrigAddress);
	if (is64Bit) {
		i686_PCI_WriteDoubleWord(address, reg + 4, 0xffffffff);
		barSizeHigh = i686_PCI_ReadDoubleWord(address, reg + 4);
		i686_PCI_WriteDoubleWord(address, reg + 4, 0);
	}
	UINT64 barSize = (((UINT64)barSizeHigh) << 32ULL) | ((UINT64)barSizeLow);
	barSize &= ~(bar->isMMIO ? (0b1111ULL) : (0b11ULL));
	barSize = ~barSize + 1ULL;
	if (barSize > 0xffffffff) {
		return false;
	}
	bar->size = (UINT32)barSize;
	bar->address = barAddress;
	return true;
}
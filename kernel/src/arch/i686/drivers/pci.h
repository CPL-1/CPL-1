#ifndef __I686_PCI_H_INCLUDED__
#define __I686_PCI_H_INCLUDED__

#include <common/misc/utils.h>

enum
{
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

struct i686_PCI_ID {
	UINT16 vendor_id;
	UINT16 device_id;
};

struct i686_PCI_Address {
	UINT8 bus;
	UINT8 slot;
	UINT8 function;
};

struct i686_pci_bar {
	UINT32 address;
	UINT32 size;

	bool isMMIO;
	bool disableCache;
};

typedef void (*i686_pci_enumerator_t)(struct i686_PCI_Address addr, struct i686_PCI_ID id, void *ctx);

void i686_PCI_EnableBusMastering(struct i686_PCI_Address address);
bool i686_PCI_ReadBAR(struct i686_PCI_Address address, int index, struct i686_pci_bar *bar);

UINT8 i686_PCI_ReadByte(struct i686_PCI_Address address, UINT8 field);
UINT16 i686_PCI_ReadWord(struct i686_PCI_Address address, UINT8 field);
UINT32 i686_PCI_ReadDoubleWord(struct i686_PCI_Address address, UINT8 field);

void i686_PCI_WriteByte(struct i686_PCI_Address address, UINT8 field, UINT8 value);
void i686_PCI_WriteWord(struct i686_PCI_Address address, UINT8 field, UINT16 value);
void i686_PCI_WriteDoubleWord(struct i686_PCI_Address address, UINT8 field, UINT32 value);

void i686_PCI_Enumerate(i686_pci_enumerator_t enumerator, void *ctx);
UINT16 i686_PCI_GetDeviceType(struct i686_PCI_Address address);

#endif
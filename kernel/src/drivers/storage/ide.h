#ifndef __IDE_H_INCLUDED__
#define __IDE_H_INCLUDED__

#include <drivers/pci.h>

#define IDE_PCI_TYPE 0x0101

void ide_init(struct pci_address addr);

#endif
#ifndef __IDE_H_INCLUDED__
#define __IDE_H_INCLUDED__

#include <drivers/pci.h>

#define NVME_PCI_TYPE 0x0108

void nvme_init(struct pci_address addr);

#endif
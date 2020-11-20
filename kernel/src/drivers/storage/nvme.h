#ifndef __IDE_H_INCLUDED__
#define __IDE_H_INCLUDED__

#include <arch/i386/drivers/pci.h>
#include <hal/drivers/storage/nvme.h>

#define NVME_PCI_TYPE 0x0108

bool nvme_init(struct hal_nvme_controller *buf);

#endif
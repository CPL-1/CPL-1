#ifndef __IDE_H_INCLUDED__
#define __IDE_H_INCLUDED__

#include <arch/i686/drivers/pci.h>
#include <hal/drivers/storage/nvme.h>

#define NVME_I686_PCI_TYPE 0x0108

bool NVME_Initialize(struct HAL_NVMEController *buf);

#endif
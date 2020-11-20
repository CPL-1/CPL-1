#ifndef __I386_NVME_H_INCLUDED__
#define __I386_NVME_H_INCLUDED__

#include <arch/i386/drivers/pci.h>
#include <hal/drivers/storage/nvme.h>
#include <hal/proc/isrhandler.h>

bool i386_nvme_detect_from_pci_bus(struct i386_pci_address addr,
								   struct hal_nvme_controller *buf);

#endif
#ifndef __I686_NVME_H_INCLUDED__
#define __I686_NVME_H_INCLUDED__

#include <arch/i686/drivers/pci.h>
#include <hal/drivers/storage/nvme.h>
#include <hal/proc/isrhandler.h>

bool i686_nvme_detect_from_pci_bus(struct i686_pci_address addr,
								   struct hal_nvme_controller *buf);

#endif
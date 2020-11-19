#include <drivers/pci.h>
#include <drivers/storage/nvme.h>
#include <init/detect.h>
#include <lib/kmsg.h>

void detect_hardware_callback(struct pci_address addr, struct pci_id id,
							  void *ctx) {
	(void)ctx;
	(void)id;
	uint16_t type = pci_get_type(addr);
	switch (type) {
	case NVME_PCI_TYPE:
		kmsg_log("Hardware Autodetection Routine",
				 "Found NVME controller on PCI bus");
		nvme_init(addr);
		break;
	default:
		break;
	}
}

void detect_hardware() { pci_enumerate(detect_hardware_callback, NULL); }
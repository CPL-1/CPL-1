#include <arch/i386/drivers/storage/nvme.h>
#include <arch/i386/init/detect.h>
#include <core/memory/heap.h>
#include <drivers/pci.h>
#include <drivers/storage/nvme.h>
#include <hal/drivers/storage/nvme.h>
#include <lib/kmsg.h>

void detect_hardware_callback(struct pci_address addr, unused struct pci_id id,
							  unused void *ctx) {
	uint16_t type = pci_get_type(addr);
	switch (type) {
	case NVME_PCI_TYPE: {
		kmsg_log("Hardware Autodetection Routine",
				 "Found NVME controller on PCI bus");
		struct hal_nvme_controller *controller =
			ALLOC_OBJ(struct hal_nvme_controller);
		if (controller == NULL) {
			kmsg_warn("Hardware Autodetection Routine",
					  "Failed to allocate NVME HAL controller object. Skipping "
					  "NVME controller");
			break;
		}
		if (!nvme_detect_from_pci_bus(addr, controller)) {
			kmsg_warn("Hardware Autodetection Routine",
					  "Failed to initialize NVME controller on PCI bus");
			break;
		}
		if (!nvme_init(controller)) {
			kmsg_warn("Hardware Autodetection Routine",
					  "Failed to initialize NVME controller on PCI bus");
		}
	} break;
	default:
		break;
	}
}

void detect_hardware() { pci_enumerate(detect_hardware_callback, NULL); }
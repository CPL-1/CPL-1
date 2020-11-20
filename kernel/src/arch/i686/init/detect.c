#include <arch/i686/drivers/pci.h>
#include <arch/i686/drivers/storage/nvme.h>
#include <arch/i686/init/detect.h>
#include <common/core/memory/heap.h>
#include <common/drivers/storage/nvme.h>
#include <common/lib/kmsg.h>
#include <hal/drivers/storage/nvme.h>

void detect_hardware_callback(struct i686_pci_address addr,
							  unused struct i686_pci_id id, unused void *ctx) {
	uint16_t type = i686_pci_get_type(addr);
	switch (type) {
	case NVME_I686_PCI_TYPE: {
		kmsg_log("i686 Hardware Autodetection Routine",
				 "Found NVME controller on PCI bus");
		struct hal_nvme_controller *controller =
			ALLOC_OBJ(struct hal_nvme_controller);
		if (controller == NULL) {
			kmsg_warn("i686 Hardware Autodetection Routine",
					  "Failed to allocate NVME HAL controller object. Skipping "
					  "NVME controller");
			break;
		}
		if (!i686_nvme_detect_from_pci_bus(addr, controller)) {
			kmsg_warn("i686 Hardware Autodetection Routine",
					  "Failed to initialize NVME controller on PCI bus");
			break;
		}
		if (!nvme_init(controller)) {
			kmsg_warn("i686 Hardware Autodetection Routine",
					  "Failed to initialize NVME controller on PCI bus");
		}
	} break;
	default:
		break;
	}
}

void detect_hardware() { i686_pci_enumerate(detect_hardware_callback, NULL); }
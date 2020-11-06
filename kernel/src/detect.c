#include <detect.h>
#include <drivers/pci.h>
#include <drivers/storage/ide.h>
#include <kmsg.h>

void detect_hardware_callback(struct pci_address addr, struct pci_id id,
                              void* ctx) {
	(void)ctx;
	(void)id;
	uint16_t type = pci_get_type(addr);
	switch (type) {
		case IDE_PCI_TYPE:
			kmsg_log("Hardware Autodetection Routine", "Found an IDE drive");
			break;
		default:
			break;
	}
}

void detect_hardware() { pci_enumerate(detect_hardware_callback, NULL); }
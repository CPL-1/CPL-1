#include <arch/i686/drivers/pci.h>
#include <arch/i686/drivers/ps2.h>
#include <arch/i686/drivers/ps2kybrd.h>
#include <arch/i686/drivers/storage/nvme.h>
#include <arch/i686/init/detect.h>
#include <common/core/memory/heap.h>
#include <common/drivers/storage/nvme.h>
#include <common/lib/kmsg.h>
#include <hal/drivers/storage/nvme.h>

void i686_DetectHardware_PCIEnumerateCallback(struct i686_PCI_Address addr, MAYBE_UNUSED struct i686_PCI_ID id,
											  MAYBE_UNUSED void *ctx) {
	uint16_t type = i686_PCI_GetDeviceType(addr);
	switch (type) {
	case NVME_I686_PCI_TYPE: {
		KernelLog_InfoMsg("i686 Hardware Autodetection Routine", "Found NVME controller on PCI bus");
		struct HAL_NVMEController *controller = ALLOC_OBJ(struct HAL_NVMEController);
		if (controller == NULL) {
			KernelLog_WarnMsg("i686 Hardware Autodetection Routine",
							  "Failed to allocate NVME HAL controller object. Skipping "
							  "NVME controller");
			break;
		}
		if (!i686_NVME_DetectFromPCIBus(addr, controller)) {
			KernelLog_WarnMsg("i686 Hardware Autodetection Routine", "Failed to initialize NVME controller on PCI bus");
			break;
		}
		if (!NVME_Initialize(controller)) {
			KernelLog_WarnMsg("i686 Hardware Autodetection Routine", "Failed to initialize NVME controller on PCI bus");
		}
	} break;
	default:
		break;
	}
}

void i686_DetectHardware_PS2EnumerateCallback(bool channel, enum i686_PS2_DeviceType type, MAYBE_UNUSED void *ctx) {
	if (type == Keyboard) {
		KernelLog_InfoMsg("i686 Hardware Autodetection Routine", "PS/2 Keyboard found on channel %d",
						  ((int)channel) + 1);
		if (!i686_PS2Keyboard_Detect(channel)) {
			KernelLog_WarnMsg("i686 Hardware Autodetection Routine", "Failed to initialize keyboard");
		}
	}
}

void i686_DetectHardware() {
	i686_PCI_Enumerate(i686_DetectHardware_PCIEnumerateCallback, NULL);
	i686_PS2_Enumerate(i686_DetectHardware_PS2EnumerateCallback, NULL);
}
#include <arch/i686/drivers/storage/nvme.h>
#include <arch/i686/proc/iowait.h>
#include <common/core/memory/heap.h>

struct i686_NVME_PCIController {
	struct i686_PCI_Address addr;
	uint8_t irq;
	struct i686_IOWait_ListEntry *entry;
	void (*eventCallback)(void *);
	void *privateCtx;
};

static bool i686_NVME_CheckInterrupt(void *ctx) {
	struct i686_NVME_PCIController *controller = (struct i686_NVME_PCIController *)ctx;
	struct i686_PCI_Address addr = controller->addr;
	uint16_t status = i686_PCI_ReadWord(addr, I686_PCI_STATUS);
	return ((status & (1 << 3)) != 0);
}

static void i686_NVME_EventCallback(void *ctx, MAYBE_UNUSED char *state) {
	struct i686_NVME_PCIController *controller = (struct i686_NVME_PCIController *)ctx;
	if (controller->eventCallback != NULL) {
		controller->eventCallback(controller->privateCtx);
	}
}

static bool i686_NVME_InitializeEvent(void *ctx, void (*eventCallback)(void *), void *privateCtx) {
	struct i686_NVME_PCIController *controller = (struct i686_NVME_PCIController *)ctx;
	uint8_t irq = i686_PCI_ReadByte(controller->addr, I686_PCI_int_LINE);
	if (irq > 15) {
		return false;
	}
	controller->irq = irq;
	controller->entry =
		i686_IOWait_AddHandler(controller->irq, i686_NVME_EventCallback, i686_NVME_CheckInterrupt, (void *)controller);
	if (controller->entry == NULL) {
		return false;
	}
	controller->eventCallback = eventCallback;
	controller->privateCtx = privateCtx;
	return true;
}

static void i686_NVME_WaitForEvent(void *ctx) {
	struct i686_NVME_PCIController *controller = (struct i686_NVME_PCIController *)ctx;
	i686_IOWait_WaitForIRQ(controller->entry);
}

bool i686_NVME_DetectFromPCIBus(struct i686_PCI_Address addr, struct HAL_NVMEController *buf) {
	struct i686_NVME_PCIController *controller = ALLOC_OBJ(struct i686_NVME_PCIController);
	if (controller == NULL) {
		return false;
	}
	controller->addr = addr;
	buf->ctx = (void *)(controller);
	struct i686_pci_bar bar;
	if (!i686_PCI_ReadBAR(addr, 0, &bar)) {
		FREE_OBJ(controller);
		return false;
	}
	i686_PCI_EnableBusMastering(addr);
	buf->offset = (uintptr_t)bar.address;
	buf->size = (size_t)bar.size;
	buf->disableCache = bar.disableCache;
	buf->initEvent = i686_NVME_InitializeEvent;
	buf->waitForEvent = i686_NVME_WaitForEvent;
	buf->ctx = controller;
	return true;
}
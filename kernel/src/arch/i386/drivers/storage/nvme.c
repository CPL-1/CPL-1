#include <arch/i386/drivers/storage/nvme.h>
#include <arch/i386/proc/iowait.h>
#include <core/memory/heap.h>

struct nvme_pci_controller {
	struct pci_address addr;
	uint8_t irq;
	struct iowait_list_entry *iowait_entry;
};

static bool nvme_check_interrupt(void *ctx) {
	struct nvme_pci_controller *controller = (struct nvme_pci_controller *)ctx;
	struct pci_address addr = controller->addr;
	uint16_t status = pci_inw(addr, PCI_STATUS);
	return ((status & (1 << 3)) != 0);
}

static bool nvme_event_init(void *ctx) {
	struct nvme_pci_controller *controller = (struct nvme_pci_controller *)ctx;
	uint8_t irq = pci_inb(controller->addr, PCI_INT_LINE);
	if (irq > 15) {
		return false;
	}
	controller->irq = irq;
	controller->iowait_entry = iowait_add_handler(
		controller->irq, NULL, nvme_check_interrupt, (void *)controller);
	if (controller->iowait_entry == NULL) {
		return false;
	}
	return true;
}

static void nvme_wait_for_event(void *ctx) {
	struct nvme_pci_controller *controller = (struct nvme_pci_controller *)ctx;
	iowait_wait(controller->iowait_entry);
}

bool nvme_detect_from_pci_bus(struct pci_address addr,
							  struct hal_nvme_controller *buf) {
	struct nvme_pci_controller *controller =
		ALLOC_OBJ(struct nvme_pci_controller);
	if (controller == NULL) {
		return false;
	}
	controller->addr = addr;
	buf->ctx = (void *)(controller);
	struct pci_bar bar;
	if (!pci_read_bar(addr, 0, &bar)) {
		FREE_OBJ(controller);
		return false;
	}
	pci_enable_bus_mastering(addr);
	buf->offset = (uintptr_t)bar.address;
	buf->size = (size_t)bar.size;
	buf->disable_cache = bar.disable_cache;
	buf->event_init = nvme_event_init;
	buf->wait_for_event = nvme_wait_for_event;
	buf->ctx = controller;
	return true;
}
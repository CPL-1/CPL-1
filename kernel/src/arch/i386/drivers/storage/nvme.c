#include <arch/i386/drivers/storage/nvme.h>
#include <arch/i386/proc/iowait.h>
#include <core/memory/heap.h>

struct i386_nvme_pci_controller {
	struct i386_pci_address addr;
	uint8_t irq;
	struct iowait_list_entry *iowait_entry;
};

static bool i386_nvme_check_interrupt(void *ctx) {
	struct i386_nvme_pci_controller *controller =
		(struct i386_nvme_pci_controller *)ctx;
	struct i386_pci_address addr = controller->addr;
	uint16_t status = i386_pci_inw(addr, I386_PCI_STATUS);
	return ((status & (1 << 3)) != 0);
}

static bool i386_nvme_event_init(void *ctx) {
	struct i386_nvme_pci_controller *controller =
		(struct i386_nvme_pci_controller *)ctx;
	uint8_t irq = i386_pci_inb(controller->addr, I386_PCI_INT_LINE);
	if (irq > 15) {
		return false;
	}
	controller->irq = irq;
	controller->iowait_entry = iowait_add_handler(
		controller->irq, NULL, i386_nvme_check_interrupt, (void *)controller);
	if (controller->iowait_entry == NULL) {
		return false;
	}
	return true;
}

static void i386_nvme_wait_for_event(void *ctx) {
	struct i386_nvme_pci_controller *controller =
		(struct i386_nvme_pci_controller *)ctx;
	iowait_wait(controller->iowait_entry);
}

bool i386_nvme_detect_from_pci_bus(struct i386_pci_address addr,
								   struct hal_nvme_controller *buf) {
	struct i386_nvme_pci_controller *controller =
		ALLOC_OBJ(struct i386_nvme_pci_controller);
	if (controller == NULL) {
		return false;
	}
	controller->addr = addr;
	buf->ctx = (void *)(controller);
	struct i386_pci_bar bar;
	if (!i386_pci_read_bar(addr, 0, &bar)) {
		FREE_OBJ(controller);
		return false;
	}
	i386_pci_enable_bus_mastering(addr);
	buf->offset = (uintptr_t)bar.address;
	buf->size = (size_t)bar.size;
	buf->disable_cache = bar.disable_cache;
	buf->event_init = i386_nvme_event_init;
	buf->wait_for_event = i386_nvme_wait_for_event;
	buf->ctx = controller;
	return true;
}
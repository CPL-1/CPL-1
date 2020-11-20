#include <arch/i686/drivers/storage/nvme.h>
#include <arch/i686/proc/iowait.h>
#include <common/core/memory/heap.h>

struct i686_nvme_pci_controller {
	struct i686_pci_address addr;
	uint8_t irq;
	struct i686_iowait_list_entry *entry;
};

static bool i686_nvme_check_interrupt(void *ctx) {
	struct i686_nvme_pci_controller *controller =
		(struct i686_nvme_pci_controller *)ctx;
	struct i686_pci_address addr = controller->addr;
	uint16_t status = i686_pci_inw(addr, I686_PCI_STATUS);
	return ((status & (1 << 3)) != 0);
}

static bool i686_nvme_event_init(void *ctx) {
	struct i686_nvme_pci_controller *controller =
		(struct i686_nvme_pci_controller *)ctx;
	uint8_t irq = i686_pci_inb(controller->addr, I686_PCI_INT_LINE);
	if (irq > 15) {
		return false;
	}
	controller->irq = irq;
	controller->entry = i686_iowait_add_handler(
		controller->irq, NULL, i686_nvme_check_interrupt, (void *)controller);
	if (controller->entry == NULL) {
		return false;
	}
	return true;
}

static void i686_nvme_wait_for_event(void *ctx) {
	struct i686_nvme_pci_controller *controller =
		(struct i686_nvme_pci_controller *)ctx;
	i686_iowait_wait(controller->entry);
}

bool i686_nvme_detect_from_pci_bus(struct i686_pci_address addr,
								   struct hal_nvme_controller *buf) {
	struct i686_nvme_pci_controller *controller =
		ALLOC_OBJ(struct i686_nvme_pci_controller);
	if (controller == NULL) {
		return false;
	}
	controller->addr = addr;
	buf->ctx = (void *)(controller);
	struct i686_pci_bar bar;
	if (!i686_pci_read_bar(addr, 0, &bar)) {
		FREE_OBJ(controller);
		return false;
	}
	i686_pci_enable_bus_mastering(addr);
	buf->offset = (uintptr_t)bar.address;
	buf->size = (size_t)bar.size;
	buf->disable_cache = bar.disable_cache;
	buf->event_init = i686_nvme_event_init;
	buf->wait_for_event = i686_nvme_wait_for_event;
	buf->ctx = controller;
	return true;
}
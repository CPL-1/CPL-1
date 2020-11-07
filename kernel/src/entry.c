#include <boot/multiboot.h>
#include <detect.h>
#include <drivers/pci.h>
#include <drivers/pic.h>
#include <drivers/pit.h>
#include <drivers/storage/storage.h>
#include <drivers/textvga.h>
#include <i386/cr3.h>
#include <i386/gdt.h>
#include <i386/idt.h>
#include <i386/tss.h>
#include <kmsg.h>
#include <memory/heap.h>
#include <memory/phys.h>
#include <memory/virt.h>
#include <proc/priv.h>
#include <proc/proc.h>
#include <proc/proclayout.h>
#include <proc/ring1.h>

void print_pci(struct pci_address addr, struct pci_id id, void *context) {
	(void)context;
	vga_set_color(0x0f);
	printf("         |> ");
	vga_set_color(0x07);
	printf("bus: %d, slot: %d, function: %d, vendor_id: %d, device_id: %d\n",
	       addr.bus, addr.slot, addr.function, id.vendor_id, id.device_id);
}

void kernel_main(uint32_t mb_offset) {
	vga_init();
	kmsg_log("Kernel Init",
	         "Preparing to unleash the real power of your CPU...");
	kmsg_init_done("VGA text display driver");
	cr3_init();
	kmsg_init_done("Root page table manager");
	gdt_init();
	kmsg_init_done("GDT loader");
	tss_init();
	kmsg_init_done("TSS loader");
	multiboot_init(mb_offset);
	kmsg_init_done("Multiboot v1.0 tables parser");
	phys_init();
	kmsg_init_done("Physical memory allocator");
	virt_kernel_mapping_init();
	kmsg_init_done("Kernel virtual memory mapper");
	heap_init();
	kmsg_init_done("Kernel heap manager");
	idt_init();
	kmsg_init_done("IDT loader");
	pic_init();
	kmsg_init_done("8259 Programmable Interrupt Controller driver");
	pit_init(25);
	kmsg_init_done("8253/8254 Programmable Interval Timer driver");
	priv_init();
	kmsg_init_done("Privilege Manager");
	ring1_switch();
	kmsg_ok("Ring 1 Initializer", "Executing in Ring 1!");
	kmsg_log("Entry Process", "Enumerating PCI Bus...");
	pci_enumerate(print_pci, NULL);
	storage_init();
	kmsg_init_done("Storage Manager");
	proc_init();
	kmsg_init_done("Process Manager & Scheduler");
	detect_hardware();
	kmsg_init_done("Hardware Autodetection Routine");
	kmsg_log("Process Manager & Scheduler", "Starting User Request Monitor...");
	while (true) {
		proc_dispose_queue_poll();
		asm volatile("pause");
	}
}
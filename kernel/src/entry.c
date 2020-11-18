#include <boot/multiboot.h>
#include <detect.h>
#include <drivers/pci.h>
#include <drivers/pic.h>
#include <drivers/pit.h>
#include <drivers/textvga.h>
#include <dynarray.h>
#include <fd/devfs.h>
#include <fd/rootfs.h>
#include <fd/vfs.h>
#include <fembed.h>
#include <i386/cr3.h>
#include <i386/gdt.h>
#include <i386/idt.h>
#include <i386/tss.h>
#include <kmsg.h>
#include <memory/heap.h>
#include <memory/phys.h>
#include <memory/virt.h>
#include <proc/intlock.h>
#include <proc/iowait.h>
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

void urm_thread() {
	while (true) {
		while (proc_dispose_queue_poll()) {
		}
		proc_yield();
	}
}

void kernel_init_process();

void kernel_main(uint32_t mb_offset) {
	intlock_lock();
	vga_init();
	kmsg_log("Kernel Init",
			 "Preparing to unleash the real power of your CPU...");
	kmsg_init_done("VGA Text Display Driver");
	cr3_init();
	kmsg_init_done("Root Page Table Manager");
	gdt_init();
	kmsg_init_done("GDT Loader");
	tss_init();
	kmsg_init_done("TSS Loader");
	multiboot_init(mb_offset);
	kmsg_init_done("Multiboot v1.0 Tables Parser");
	phys_init();
	kmsg_init_done("Physical Memory Allocator");
	virt_kernel_mapping_init();
	kmsg_init_done("Kernel Virtual Memory Mapper");
	heap_init();
	kmsg_init_done("Kernel Heap Manager");
	iowait_init();
	kmsg_init_done("IO Wait Subsystem");
	idt_init();
	kmsg_init_done("IDT Loader");
	pic_init();
	kmsg_init_done("8259 Programmable Interrupt Controller Driver");
	pit_init(25);
	kmsg_init_done("8253/8254 Programmable Interval Timer Driver");
	priv_init();
	kmsg_init_done("Privilege Manager");
	ring1_switch();
	kmsg_ok("Ring 1 Initializer", "Executing in Ring 1!");
	kmsg_log("Entry Process", "Enumerating PCI Bus...");
	pci_enumerate(print_pci, NULL);
	proc_init();
	kmsg_init_done("Process Manager & Scheduler");
	vfs_init(rootfs_make_superblock());
	kmsg_init_done("Virtual File System");
	devfs_init();
	kmsg_init_done("Device File System");
	vfs_mount_user("/dev/", NULL, "devfs");
	kmsg_log("Kernel Init", "Mounted Device Filesystem on /dev/");
	detect_hardware();
	kmsg_init_done("Hardware Autodetection Routine");
	intlock_unlock();
	iowait_enable_used_irq();
	kmsg_log("IO wait subsystem", "Interrupts enabled. IRQ will now fire");
	kmsg_log("Process Manager & Scheduler", "Starting User Request Monitor...");
	struct proc_id urm_id = proc_new_process(proc_my_id());
	struct proc_process *urm_data = proc_get_data(urm_id);
	urm_data->frame.ds = urm_data->frame.es = urm_data->frame.gs =
		urm_data->frame.fs = urm_data->frame.ss = 0x21;
	urm_data->frame.cs = 0x19;
	urm_data->frame.eip = (uint32_t)kernel_init_process;
	urm_data->frame.esp = urm_data->kernel_stack + PROC_KERNEL_STACK_SIZE;
	urm_data->frame.eflags = (1 << 9) | (1 << 12);
	proc_continue(urm_id);
	urm_thread();
}

void kernel_init_process() {
	kmsg_log("Kernel Init", "Executing in a separate init process");
	kmsg_log("Kernel Init", "Listing active devices");
	struct fd *nvme_fd = vfs_open("/dev/nvme0n1", 0);
	if (nvme_fd == NULL) {
		kmsg_err("Kernel Init", "Failed to open /dev/nvme0n1");
	}
	char buf[] = "IF YOU SEE THIS STRING, NVME DRIVER, VFS, AND STORAGE STACK ARE FUNCTIONING AS THEY SHOULD";
	fd_lseek(nvme_fd, 0, 0);
	if (fd_write(nvme_fd, sizeof(buf) - 1, buf) != sizeof(buf) - 1) {
		kmsg_err("Kernel Init",
				 "Failed to read 500 bytes from disk /dev/nvme0n1");
	}
	while (true)
		;
}
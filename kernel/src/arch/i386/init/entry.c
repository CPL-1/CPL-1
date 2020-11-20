#include <arch/i386/cpu/cr3.h>
#include <arch/i386/cpu/gdt.h>
#include <arch/i386/cpu/idt.h>
#include <arch/i386/cpu/tss.h>
#include <arch/i386/drivers/pci.h>
#include <arch/i386/drivers/pic.h>
#include <arch/i386/drivers/pit.h>
#include <arch/i386/drivers/tty.h>
#include <arch/i386/init/detect.h>
#include <arch/i386/init/multiboot.h>
#include <arch/i386/memory/phys.h>
#include <arch/i386/memory/virt.h>
#include <arch/i386/proc/iowait.h>
#include <arch/i386/proc/priv.h>
#include <arch/i386/proc/ring1.h>
#include <arch/i386/proc/state.h>
#include <core/fd/fs/devfs.h>
#include <core/fd/fs/rootfs.h>
#include <core/fd/vfs.h>
#include <core/memory/heap.h>
#include <core/proc/proc.h>
#include <core/proc/proclayout.h>
#include <hal/drivers/tty.h>
#include <hal/memory/phys.h>
#include <hal/memory/virt.h>
#include <hal/proc/intlock.h>
#include <hal/proc/isrhandler.h>
#include <lib/dynarray.h>
#include <lib/kmsg.h>

void print_pci(struct i386_pci_address addr, struct i386_pci_id id,
			   void *context) {
	(void)context;
	hal_tty_set_color(0x0f);
	printf("         |> ");
	hal_tty_set_color(0x07);
	printf("bus: %d, slot: %d, function: %d, vendor_id: %d, device_id: %d\n",
		   addr.bus, addr.slot, addr.function, id.vendor_id, id.device_id);
}

void urm_thread() {
	while (true) {
		while (proc_dispose_queue_poll()) {
		}
		asm volatile("pause");
		proc_yield();
	}
}

void kernel_init_process();

void i386_kernel_main(uint32_t mb_offset) {
	hal_intlock_lock();
	i386_tty_init();
	kmsg_log("Kernel Init",
			 "Preparing to unleash the real power of your CPU...");
	kmsg_init_done("VGA Text Display Driver");
	i386_cr3_init();
	kmsg_init_done("Root Page Table Manager");
	i386_gdt_init();
	kmsg_init_done("GDT Loader");
	i386_tss_init();
	kmsg_init_done("TSS Loader");
	multiboot_init(mb_offset);
	kmsg_init_done("Multiboot v1.0 Tables Parser");
	i386_phys_init();
	kmsg_init_done("Physical Memory Allocator");
	i386_virt_kernel_mapping_init();
	kmsg_init_done("Kernel Virtual Memory Mapper");
	heap_init();
	kmsg_init_done("Kernel Heap Manager");
	iowait_init();
	kmsg_init_done("IO Wait Subsystem");
	i386_idt_init();
	kmsg_init_done("IDT Loader");
	i386_pic_init();
	kmsg_init_done("8259 Programmable Interrupt Controller Driver");
	i386_pit_init(25);
	kmsg_init_done("8253/8254 Programmable Interval Timer Driver");
	priv_init();
	kmsg_init_done("Privilege Manager");
	ring1_switch();
	kmsg_ok("Ring 1 Initializer", "Executing in Ring 1!");
	kmsg_log("Entry Process", "Enumerating PCI Bus...");
	i386_pci_enumerate(print_pci, NULL);
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
	hal_intlock_unlock();
	iowait_enable_used_irq();
	kmsg_log("IO wait subsystem", "Interrupts enabled. IRQ will now fire");
	kmsg_log("Process Manager & Scheduler", "Starting User Request Monitor...");
	struct proc_id init_id = proc_new_process(proc_my_id());
	struct proc_process *init_data = proc_get_data(init_id);
	struct i386_cpu_state *init_state =
		(struct i386_cpu_state *)(init_data->process_state);
	init_state->ds = init_state->es = init_state->gs = init_state->fs =
		init_state->ss = 0x21;
	init_state->cs = 0x19;
	init_state->eip = (uint32_t)kernel_init_process;
	init_state->esp = init_data->kernel_stack + PROC_KERNEL_STACK_SIZE;
	init_state->eflags = (1 << 9) | (1 << 12);
	proc_continue(init_id);
	urm_thread();
}

void kernel_init_process() {
	kmsg_log("Kernel Init", "Executing in a separate init process");
	while (true)
		;
}
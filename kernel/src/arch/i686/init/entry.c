#include <arch/i686/cpu/cr3.h>
#include <arch/i686/cpu/gdt.h>
#include <arch/i686/cpu/idt.h>
#include <arch/i686/cpu/tss.h>
#include <arch/i686/drivers/pci.h>
#include <arch/i686/drivers/pic.h>
#include <arch/i686/drivers/pit.h>
#include <arch/i686/drivers/tty.h>
#include <arch/i686/init/detect.h>
#include <arch/i686/init/multiboot.h>
#include <arch/i686/memory/phys.h>
#include <arch/i686/memory/virt.h>
#include <arch/i686/proc/iowait.h>
#include <arch/i686/proc/priv.h>
#include <arch/i686/proc/ring1.h>
#include <arch/i686/proc/state.h>
#include <common/core/fd/fs/devfs.h>
#include <common/core/fd/fs/fat32.h>
#include <common/core/fd/fs/rootfs.h>
#include <common/core/fd/vfs.h>
#include <common/core/memory/heap.h>
#include <common/core/proc/proc.h>
#include <common/core/proc/proclayout.h>
#include <common/lib/dynarray.h>
#include <common/lib/kmsg.h>
#include <hal/drivers/tty.h>
#include <hal/memory/phys.h>
#include <hal/memory/virt.h>
#include <hal/proc/intlock.h>
#include <hal/proc/isrhandler.h>

void print_pci(struct i686_pci_address addr, struct i686_pci_id id,
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

void i686_kernel_main(uint32_t mb_offset) {
	i686_tty_init();
	kmsg_log("i686 Kernel Init",
			 "Preparing to unleash the real power of your CPU...");
	kmsg_init_done("i686 VGA Text Display Driver");
	i686_cr3_init();
	kmsg_init_done("i686 Root Page Table Manager");
	i686_gdt_init();
	kmsg_init_done("i686 GDT Loader");
	i686_tss_init();
	kmsg_init_done("i686 TSS Loader");
	i686_multiboot_init(mb_offset);
	kmsg_init_done("i686 Multiboot v1.0 Tables Parser");
	i686_phys_init();
	kmsg_init_done("i686 Physical Memory Allocator");
	i686_virt_kernel_mapping_init();
	kmsg_init_done("i686 Virtual Memory Mapper");
	heap_init();
	kmsg_init_done("Kernel Heap Heap");
	i686_iowait_init();
	kmsg_init_done("i686 IO Wait Subsystem");
	i686_idt_init();
	kmsg_init_done("i686 IDT Loader");
	i686_pic_init();
	kmsg_init_done("i686 8259 Programmable Interrupt Controller Driver");
	i686_pit_init(25);
	kmsg_init_done("8253/8254 Programmable Interval Timer Driver");
	i686_priv_init();
	kmsg_init_done("i686 Privilege Manager");
	ring1_switch();
	kmsg_ok("i686 Ring 1 Initializer", "Executing in Ring 1!");
	proc_init();
	kmsg_init_done("Process Manager & Scheduler");
	vfs_init(rootfs_make_superblock());
	kmsg_log("i686 Kernel Init", "Starting Init Process...");
	struct proc_id init_id = proc_new_process(proc_my_id());
	struct proc_process *init_data = proc_get_data(init_id);
	struct i686_cpu_state *init_state =
		(struct i686_cpu_state *)(init_data->process_state);
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
	kmsg_log("i686 Kernel Init", "Executing in a separate init process");
	kmsg_log("Entry Process", "Enumerating PCI Bus...");
	i686_pci_enumerate(print_pci, NULL);
	kmsg_init_done("Virtual File System");
	devfs_init();
	kmsg_init_done("Device File System");
	fat32_init();
	kmsg_init_done("FAT32 File System");
	vfs_mount_user("/dev/", NULL, "devfs");
	kmsg_log("i686 Kernel Init", "Mounted Device Filesystem on /dev/");
	i686_iowait_enable_used_irq();
	kmsg_log("i686 IO wait subsystem", "Interrupts enabled. IRQ will now fire");
	detect_hardware();
	kmsg_init_done("i686 Hardware Autodetection Routine");
	vfs_mount_user("/", "/dev/nvme0n1p1", "fat32");
	kmsg_log("i686 Kernel Init", "Mounted FAT32 Filesystem on /");
	while (true)
		;
}
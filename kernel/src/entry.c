#include <boot/multiboot.h>
#include <drivers/pic.h>
#include <drivers/pit.h>
#include <drivers/textvga.h>
#include <i386/cr3.h>
#include <i386/gdt.h>
#include <i386/idt.h>
#include <i386/tss.h>
#include <kmsg.h>
#include <memory/heap.h>
#include <memory/phys.h>
#include <memory/virt.h>
#include <proc/proc.h>
#include <proc/proclayout.h>
#include <proc/ring1.h>

void test_process() {
	for (size_t i = 0; i < 5; ++i) {
		kmsg_log("Test Process No 1", "Loud and Clear!");
		for (size_t i = 0; i < 1000000; ++i) {
			asm volatile("pause");
		}
	}
	proc_exit(69);
}

void kernel_main(uint32_t mb_offset) {
	vga_init();
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
	kmsg_init_done("8253/8254 Programmable Interval Timer driver ");
	ring1_switch();
	kmsg_ok("Ring 1 Initializer", "Executing in Ring 1!");
	proc_init();
	kmsg_init_done("Process Manager & Scheduler");
	kmsg_log("Process Manager & Scheduler", "Starting User Request Monitor");
	/* TEST CODE */
	struct proc_id new_process = proc_new_process(proc_my_id());
	struct proc_process *process_data = proc_get_data(new_process);
	process_data->frame.cs = 0x19;
	process_data->frame.ds = process_data->frame.ss = process_data->frame.es =
	    process_data->frame.fs = process_data->frame.gs = 0x21;
	process_data->frame.eip = (uint32_t)(test_process);
	process_data->frame.esp =
	    (uint32_t)(process_data->kernel_stack + PROC_KERNEL_STACK_SIZE);
	process_data->frame.eflags = (1 << 9) | (1 << 12);
	proc_continue(new_process);
	kmsg_log("Kernel Init", "Waiting for Test Process 1");
	struct proc_process *process = proc_wait_for_child_term();
	printf("Process 1 Terminated. Exit code: %d\n", process->return_code);
	kmsg_log("Kernel Init", "Disposing process");
	proc_dispose(process);
	while (true) {
		proc_dispose_queue_poll();
		asm volatile("pause");
	}
}

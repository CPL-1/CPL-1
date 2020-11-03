#include <boot/multiboot.h>
#include <drivers/pic.h>
#include <drivers/pit.h>
#include <drivers/textvga.h>
#include <i386/gdt.h>
#include <i386/idt.h>
#include <i386/tss.h>
#include <kmsg.h>
#include <memory/heap.h>
#include <memory/phys.h>
#include <memory/virt.h>
#include <proc/proc.h>
#include <proc/ring1.h>

void test() {
	while (true) {
		for (size_t i = 0; i < 500000000; ++i) {
			asm volatile("nop");
		}
		printf("ok\n");
	}
}

void kernel_main(uint32_t mb_offset) {
	vga_init();
	kmsg_init_done("VGA text display driver");
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
	kmsg_init_done("Process manager & Scheduler");
	proc_start_new_kernel_thread((uint32_t)test);
	while (true) {
		for (size_t i = 0; i < 500000000; ++i) {
			asm volatile("nop");
		}
		printf("ok\n");
	}
}
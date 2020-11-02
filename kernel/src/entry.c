#include <boot/multiboot.h>
#include <drivers/pic.h>
#include <drivers/pit.h>
#include <drivers/textvga.h>
#include <i386/gdt.h>
#include <i386/idt.h>
#include <kmsg.h>
#include <memory/heap.h>
#include <memory/phys.h>
#include <memory/virt.h>
#include <proc/ring1.h>

void test() { printf("Lol, hello from int handler\n\n\n\n\n"); }

void kernel_main(uint32_t mb_offset) {
	vga_init();
	kmsg_init_done("VGA text display driver");
	gdt_init();
	kmsg_init_done("GDT loader");
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
	while (true)
		;
}
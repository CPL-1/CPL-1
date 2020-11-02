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
	kmsg_init_done("vga");
	gdt_init();
	kmsg_init_done("gdt");
	multiboot_init(mb_offset);
	kmsg_init_done("mb");
	phys_init();
	kmsg_init_done("physical");
	virt_kernel_mapping_init();
	kmsg_init_done("virt");
	heap_init();
	kmsg_init_done("heap");
	idt_init();
	kmsg_init_done("idt");
	pic_init();
	kmsg_init_done("pic");
	pit_init(25);
	kmsg_init_done("pit");
	ring1_switch();
	kmsg_init_done("ring1");
}
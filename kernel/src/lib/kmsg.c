#include <drivers/textvga.h>
#include <lib/kmsg.h>

void kmsg_init_done(const char *mod) {
	printf("[ ");
	vga_set_color(0x0a);
	printf("OKAY");
	vga_set_color(0x07);
	printf(" ] Target ");
	vga_set_color(0x0f);
	printf("%s", mod);
	vga_set_color(0x07);
	printf(" reached\n");
}

void kmsg_ok(const char *mod, const char *text) {
	printf("[ ");
	vga_set_color(0x0a);
	printf("OKAY");
	vga_set_color(0x07);
	printf(" ] ");
	vga_set_color(0x0f);
	printf("%s: ", mod);
	vga_set_color(0x07);
	printf("%s\n", text);
}

void kmsg_warn(const char *mod, const char *text) {
	printf("[ ");
	vga_set_color(0x0e);
	printf("WARN");
	vga_set_color(0x07);
	printf(" ] ");
	vga_set_color(0x0f);
	printf("%s: ", mod);
	vga_set_color(0x07);
	printf("%s\n", text);
}

void kmsg_err(const char *mod, const char *text) {
	asm volatile("cli");
	printf("[ ");
	vga_set_color(0x0c);
	printf("FAIL");
	vga_set_color(0x07);
	printf(" ] ");
	vga_set_color(0x0f);
	printf("%s: ", mod);
	vga_set_color(0x07);
	printf("%s\n", text);
	while (true) {
		asm volatile("pause");
	}
}

void kmsg_log(const char *mod, const char *text) {
	printf("[ ");
	vga_set_color(0x09);
	printf("INFO");
	vga_set_color(0x07);
	printf(" ] ");
	vga_set_color(0x0f);
	printf("%s: ", mod);
	vga_set_color(0x07);
	printf("%s\n", text);
}

#include <drivers/textvga.h>
#include <kmsg.h>

void kmsg_init_done(const char *mod) {
	printf("[ ");
	vga_set_color(0x0a);
	printf("OKAY");
	vga_set_color(0x07);
	printf(" ] Started ");
	vga_set_color(0x0f);
	printf("%s\n", mod);
	vga_set_color(0x07);
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
	printf("[ ");
	vga_set_color(0x0c);
	printf("FAIL");
	vga_set_color(0x07);
	printf(" ] ");
	vga_set_color(0x0f);
	printf("%s: ", mod);
	vga_set_color(0x07);
	printf("%s\n", text);
	asm volatile("cli");
	while (true) {
		asm volatile("hlt");
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
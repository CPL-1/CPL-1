#include <common/lib/kmsg.h>
#include <hal/drivers/tty.h>

void kmsg_init_done(const char *mod) {
	printf("[ ");
	hal_tty_set_color(0x0a);
	printf("OKAY");
	hal_tty_set_color(0x07);
	printf(" ] Target ");
	hal_tty_set_color(0x0f);
	printf("%s", mod);
	hal_tty_set_color(0x07);
	printf(" reached\n");
}

void kmsg_ok(const char *mod, const char *text) {
	printf("[ ");
	hal_tty_set_color(0x0a);
	printf("OKAY");
	hal_tty_set_color(0x07);
	printf(" ] ");
	hal_tty_set_color(0x0f);
	printf("%s: ", mod);
	hal_tty_set_color(0x07);
	printf("%s\n", text);
}

void kmsg_warn(const char *mod, const char *text) {
	printf("[ ");
	hal_tty_set_color(0x0e);
	printf("WARN");
	hal_tty_set_color(0x07);
	printf(" ] ");
	hal_tty_set_color(0x0f);
	printf("%s: ", mod);
	hal_tty_set_color(0x07);
	printf("%s\n", text);
}

void kmsg_err(const char *mod, const char *text) {
	asm volatile("cli");
	printf("[ ");
	hal_tty_set_color(0x0c);
	printf("FAIL");
	hal_tty_set_color(0x07);
	printf(" ] ");
	hal_tty_set_color(0x0f);
	printf("%s: ", mod);
	hal_tty_set_color(0x07);
	printf("%s\n", text);
	while (true) {
		asm volatile("pause");
	}
}

void kmsg_log(const char *mod, const char *text) {
	printf("[ ");
	hal_tty_set_color(0x09);
	printf("INFO");
	hal_tty_set_color(0x07);
	printf(" ] ");
	hal_tty_set_color(0x0f);
	printf("%s: ", mod);
	hal_tty_set_color(0x07);
	printf("%s\n", text);
}

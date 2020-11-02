#ifndef __KMSG_H_INCLUDED__
#define __KMSG_H_INCLUDED__

#include <utils.h>

inline static void kmsg_init_done(const char *mod) {
	printf("[%s] Module initialized successfully!\n", mod);
}

inline static void kmsg_warn(const char *mod, const char *text) {
	printf("[%s] Warning: \"%s\"\n", mod, text);
}

inline static void kmsg_err(const char *mod, const char *text) {
	printf("[%s] Error: \"%s\"\n", mod, text);
	while (true) {
		asm volatile("hlt");
	}
}

inline static void kmsg_log(const char *mod, const char *text) {
	printf("[%s] %s", mod, text);
}

#endif
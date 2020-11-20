#include <common/misc/utils.h>
#include <hal/proc/intlock.h>

static size_t i386_intlocks_count = 0;

void hal_intlock_lock() {
	asm volatile("cli");
	++i386_intlocks_count;
}

void hal_intlock_unlock() {
	if (i386_intlocks_count == 0) {
		return;
	}
	--i386_intlocks_count;
	if (i386_intlocks_count == 0) {
		asm volatile("sti");
	}
}

void hal_intlock_flush() {
	i386_intlocks_count = 0;
	asm volatile("sti");
}
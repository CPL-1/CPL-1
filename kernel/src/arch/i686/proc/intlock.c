#include <common/misc/utils.h>
#include <hal/proc/intlock.h>

static size_t i686_intlocks_count = 0;

void hal_intlock_lock() {
	asm volatile("cli");
	++i686_intlocks_count;
}

void hal_intlock_unlock() {
	if (i686_intlocks_count == 0) {
		return;
	}
	--i686_intlocks_count;
	if (i686_intlocks_count == 0) {
		asm volatile("sti");
	}
}

void hal_intlock_flush() {
	i686_intlocks_count = 0;
	asm volatile("sti");
}
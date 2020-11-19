#include <core/proc/intlock.h>

static size_t ints_count = 0;

void intlock_lock() {
	asm volatile("cli");
	++ints_count;
}

void intlock_unlock() {
	if (ints_count == 0) {
		return;
	}
	--ints_count;
	if (ints_count == 0) {
		asm volatile("sti");
	}
}

void intlock_flush() {
	ints_count = 0;
	asm volatile("sti");
}

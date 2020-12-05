#include <common/misc/utils.h>
#include <hal/proc/intlock.h>

static size_t i686_InterruptLock_LocksCount = 0;

void HAL_InterruptLock_Lock() {
	ASM volatile("cli");
	++i686_InterruptLock_LocksCount;
}

void HAL_InterruptLock_Unlock() {
	if (i686_InterruptLock_LocksCount == 0) {
		return;
	}
	--i686_InterruptLock_LocksCount;
	if (i686_InterruptLock_LocksCount == 0) {
		ASM volatile("sti");
	}
}

void HAL_InterruptLock_Flush() {
	i686_InterruptLock_LocksCount = 0;
	ASM volatile("sti");
}
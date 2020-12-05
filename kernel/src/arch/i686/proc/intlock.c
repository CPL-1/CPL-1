#include <common/misc/utils.h>
#include <hal/proc/intlock.h>

static size_t m_locksCount = 0;

void HAL_InterruptLock_Lock() {
	ASM volatile("cli");
	++m_locksCount;
}

void HAL_InterruptLock_Unlock() {
	if (m_locksCount == 0) {
		return;
	}
	--m_locksCount;
	if (m_locksCount == 0) {
		ASM volatile("sti");
	}
}

void HAL_InterruptLock_Flush() {
	m_locksCount = 0;
	ASM volatile("sti");
}
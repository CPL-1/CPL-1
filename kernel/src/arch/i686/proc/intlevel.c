#include <common/misc/utils.h>
#include <hal/proc/intlevel.h>

int HAL_InterruptLevel_Elevate() {
	uint32_t flags;
	ASM volatile("pushf\n\tpop %0" : "=g"(flags));
	if ((flags & (1 << 9)) != 0) {
		ASM volatile("cli");
		return 0;
	}
	return 1;
}

void HAL_InterruptLevel_Recover(int level) {
	if (level == 0) {
		ASM volatile("sti");
	}
}
#include <arch/i686/cpu/idt.h>
#include <arch/i686/cpu/ports.h>
#include <arch/i686/drivers/pic.h>
#include <arch/i686/drivers/pit.h>
#include <arch/i686/proc/iowait.h>
#include <arch/i686/proc/isrhandler.h>
#include <hal/proc/isrhandler.h>

void i686_PIT8253_Initialize(uint32_t freq) {
	uint32_t divisor = 1193180 / freq;
	i686_Ports_WriteByte(0x43, 0x36);

	uint8_t lo = (uint8_t)(divisor & 0xff);
	uint8_t hi = (uint8_t)((divisor >> 8) & 0xff);

	i686_Ports_WriteByte(0x40, lo);
	i686_Ports_WriteByte(0x40, hi);
}

bool HAL_Timer_SetCallback(HAL_ISR_Handler entry) {
	i686_IOWait_AddHandler(0, (i686_iowait_handler_t)entry, NULL, NULL);
	HAL_ISR_Handler handler = i686_ISR_MakeNewISRHandler(entry, NULL);
	if (handler == NULL) {
		return false;
	}
	i686_IDT_InstallISR(0xfe, (uint32_t)handler);
	return true;
}

void HAL_Timer_TriggerInterrupt() {
	ASM VOLATILE("int $0xfe");
}
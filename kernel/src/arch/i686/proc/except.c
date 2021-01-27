#include <arch/i686/cpu/idt.h>
#include <arch/i686/proc/except.h>
#include <arch/i686/proc/isrhandler.h>
#include <arch/i686/proc/state.h>
#include <common/lib/kmsg.h>

static const char *m_exceptionNames[0x20] = {"Divide-by-zero Error",
											 "Debug",
											 "Non-maskable Interrupt",
											 "Breakpoint",
											 "Overflow",
											 "Bound Range Exceeded",
											 "Invalid Opcode",
											 "Device Not Available",
											 "Coprocessor Segment Overrun",
											 "Invalid TSS",
											 "Segment Not Present",
											 "Stack-Segment Fault",
											 "General Protection Fault",
											 "Page Fault",
											 "Reserved",
											 "x87 Floating_Point Exception",
											 "Alignment Check",
											 "Machine Check",
											 "SIMD Floating-Point Exception",
											 "Virtualization Exception",
											 "Reserved"};

void i686_ExceptionMonitor_ExceptionHandler(void *ctx, char *frame) {
	struct i686_CPUState *state = (struct i686_CPUState *)frame;
	KernelLog_ErrorMsg("CPU Exception monitor", "Unhandled interrupt: \"%s\" (%u). EIP: %p, ESP: %p, EFLAGS: %p\n",
					   *(const char **)ctx, (((uint32_t)ctx) - ((uint32_t)m_exceptionNames)) / 4, state->eip,
					   state->esp, state->eflags);
}

void i686_ExceptionMonitor_Initialize() {
	for (size_t i = 0; i < 0x20; ++i) {
		HAL_ISR_Handler handler =
			i686_ISR_MakeNewISRHandler(i686_ExceptionMonitor_ExceptionHandler, (void *)(m_exceptionNames + i));
		i686_IDT_InstallISR(i, (uint32_t)handler);
	}
}
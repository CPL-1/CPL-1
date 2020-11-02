#include <drivers/pit.h>
#include <i386/tss.h>
#include <kmsg.h>
#include <proc/proc.h>

struct proc_trap_frame {
	uint32_t edi, esi, ebp;
	uint32_t : 32;
	uint32_t ebx, edx, ecx, eax;
	uint32_t ds, gs, fs, es;
	uint32_t eip, cs, eflags, esp, ss;
} packed;

#define PROC_STACK_SIZE 4096
static char proc_stack[PROC_STACK_SIZE];

static void proc_print_eax(struct proc_trap_frame *frame) {
	printf("%p\n", frame->eax);
	while (true)
		;
}

extern void proc_test();

void proc_init() {
	tss_set_dpl0_stack((uint32_t)proc_stack, 0x10);
	pit_set_callback((uint32_t)proc_print_eax);
}

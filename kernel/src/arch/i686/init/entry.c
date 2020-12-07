#include <arch/i686/cpu/cr3.h>
#include <arch/i686/cpu/gdt.h>
#include <arch/i686/cpu/idt.h>
#include <arch/i686/cpu/tss.h>
#include <arch/i686/drivers/pci.h>
#include <arch/i686/drivers/pic.h>
#include <arch/i686/drivers/pit.h>
#include <arch/i686/drivers/tty.h>
#include <arch/i686/init/detect.h>
#include <arch/i686/init/stivale.h>
#include <arch/i686/memory/phys.h>
#include <arch/i686/memory/virt.h>
#include <arch/i686/proc/elf32.h>
#include <arch/i686/proc/iowait.h>
#include <arch/i686/proc/priv.h>
#include <arch/i686/proc/ring1.h>
#include <arch/i686/proc/ring3.h>
#include <arch/i686/proc/state.h>
#include <common/core/fd/fdtable.h>
#include <common/core/fd/fs/devfs.h>
#include <common/core/fd/fs/fat32.h>
#include <common/core/fd/fs/rootfs.h>
#include <common/core/fd/vfs.h>
#include <common/core/memory/heap.h>
#include <common/core/memory/virt.h>
#include <common/core/proc/elf32.h>
#include <common/core/proc/proc.h>
#include <common/core/proc/proclayout.h>
#include <common/lib/dynarray.h>
#include <common/lib/kmsg.h>
#include <hal/drivers/tty.h>
#include <hal/memory/phys.h>
#include <hal/memory/virt.h>
#include <hal/proc/intlock.h>
#include <hal/proc/isrhandler.h>

void i686_KernelInit_DisplayPCIDevice(struct i686_PCI_Address addr, struct i686_PCI_ID id, MAYBE_UNUSED void *context) {
	KernelLog_Print("PCI device found at bus: %d, slot: %d, function: %d, "
					"vendor_id: %d, device_id: %d",
					addr.bus, addr.slot, addr.function, id.vendor_id, id.device_id);
}

void i686_KernelInit_URMThreadFunction() {
	while (true) {
		while (Proc_PollDisposeQueue()) {
		}
		ASM volatile("pause");
		Proc_Yield();
	}
}

void i686_KernelInit_ExecuteInitProcess();

void i686_KernelInit_Main(uint32_t mb_offset) {
	i686_Stivale_Initialize(mb_offset);
	KernelLog_InfoMsg("i686 Kernel Init", "Preparing to unleash the real power of your CPU...");
	KernelLog_InitDoneMsg("i686 Stivale v1.0 Tables Parser");
	i686_CR3_Initialize();
	KernelLog_InitDoneMsg("i686 Root Page Table Manager");
	i686_GDT_Initialize();
	KernelLog_InitDoneMsg("i686 GDT Loader");
	i686_TSS_Initialize();
	KernelLog_InitDoneMsg("i686 TSS Loader");
	i686_PhysicalMM_Initialize();
	KernelLog_InitDoneMsg("i686 Physical Memory Allocator");
	i686_VirtualMM_InitializeKernelMap();
	KernelLog_InitDoneMsg("i686 Virtual Memory Mapper");
	Heap_Initialize();
	KernelLog_InitDoneMsg("Kernel Heap Heap");
	i686_IOWait_Initialize();
	KernelLog_InitDoneMsg("i686 IO Wait Subsystem");
	i686_IDT_Initialize();
	KernelLog_InitDoneMsg("i686 IDT Loader");
	i686_PIC8259_Initialize();
	KernelLog_InitDoneMsg("i686 8259 Programmable Interrupt Controller Driver");
	i686_PIT8253_Initialize(25);
	KernelLog_InitDoneMsg("8253/8254 Programmable Interval Timer Driver");
	i686_Ring0Executor_Initialize();
	KernelLog_InitDoneMsg("i686 Privilege Manager");
	i686_Ring1_Switch();
	KernelLog_OkMsg("i686 Ring 1 Initializer", "Executing in Ring 1!");
	Proc_Initialize();
	KernelLog_InitDoneMsg("Process Manager & Scheduler");
	VFS_Initialize(RootFS_MakeSuperblock());
	i686_TTY_Initialize();
	KernelLog_InitDoneMsg("i686 Terminal");
	i686_Ring3_SyscallInit();
	KernelLog_InitDoneMsg("i686 System Call Interface");
	KernelLog_InfoMsg("i686 Kernel Init", "Starting Init Process...");
	struct Proc_ProcessID initID = Proc_MakeNewProcess(Proc_GetProcessID());
	struct Proc_Process *initData = Proc_GetProcessData(initID);
	struct Proc_ProcessID selfID = Proc_GetProcessID();
	struct Proc_Process *selfData = Proc_GetProcessData(selfID);
	struct i686_CPUState *initState = (struct i686_CPUState *)(initData->processState);
	initState->ds = initState->es = initState->gs = initState->fs = initState->ss = 0x21;
	initState->cs = 0x19;
	initState->eip = (uint32_t)i686_KernelInit_ExecuteInitProcess;
	initState->esp = initData->kernelStack + PROC_KERNEL_STACK_SIZE;
	initState->eflags = (1 << 9) | (1 << 12);
	initData->addressSpace = VirtualMM_ReferenceAddressSpace(selfData->addressSpace);
	if (initData->addressSpace == NULL) {
		KernelLog_ErrorMsg("i686 Kernel Init", "Failed to allocate address space object");
	}
	initData->fdTable = FileTable_MakeNewTable();
	if (initData->fdTable == NULL) {
		KernelLog_ErrorMsg("i686 Kernel Init", "Failed to allocate file table object");
	}
	Proc_Resume(initID);
	KernelLog_InfoMsg("i686 Kernel Init", "Init process summoned");
	i686_KernelInit_URMThreadFunction();
}

void i686_KernelInit_ExecuteInitProcess() {
	KernelLog_InfoMsg("i686 Kernel Init", "Executing in a separate init process");
	KernelLog_InfoMsg("Entry Process", "Enumerating PCI Bus...");
	i686_PCI_Enumerate(i686_KernelInit_DisplayPCIDevice, NULL);
	KernelLog_InitDoneMsg("Virtual File System");
	DevFS_Initialize();
	KernelLog_InitDoneMsg("Device File System");
	FAT32_Initialize();
	KernelLog_InitDoneMsg("FAT32 File System");
	if (!VFS_UserMount("/dev/", NULL, "devfs")) {
		KernelLog_ErrorMsg("i686 Kernel Init", "Failed to mount Device Filesystem on /dev/");
	}
	KernelLog_InfoMsg("i686 Kernel Init", "Mounted Device Filesystem on /dev/");
	i686_IOWait_UnmaskUsedIRQ();
	KernelLog_InfoMsg("i686 IO wait subsystem", "Interrupts enabled. IRQ will now fire");
	i686_DetectHardware();
	KernelLog_InitDoneMsg("i686 Hardware Autodetection Routine");
	if (!VFS_UserMount("/", "/dev/nvme0n1p1", "fat32")) {
		KernelLog_ErrorMsg("i686 Kernel Init", "Failed to mount FAT32 Filesystem on /");
	}
	KernelLog_InfoMsg("i686 Kernel Init", "Mounted FAT32 Filesystem on /");
	if (!VFS_UserMount("/dev/", NULL, "devfs")) {
		KernelLog_ErrorMsg("i686 Kernel Init", "Failed to remount Device Filesystem on /dev/");
	}
	KernelLog_InfoMsg("i686 Kernel Init", "Remounted Device Filesystem on /dev/");
	KernelLog_InfoMsg("i686 Kernel Init", "Loading \"/bin/init\" executable");
	struct File *file = VFS_Open("/bin/init", VFS_O_RDONLY);
	if (file == NULL) {
		KernelLog_ErrorMsg("i686 Kernel Init", "Failed to open init binary file");
	}
	struct Elf32 *elf = Elf32_Parse(file, i686_Elf32_HeaderVerifyCallback);
	if (elf == NULL) {
		KernelLog_ErrorMsg("i686 Kernel Init", "Failed to parse init binary");
	}
	if (!Elf32_LoadProgramHeaders(file, elf)) {
		KernelLog_ErrorMsg("i686 Kernel Init", "Failed to load init binary in memory");
	}
	KernelLog_InfoMsg("i686 Kernel Init", "Init binary is loaded. Jumping to the entrypoint");
	i686_Ring3_Switch(elf->entryPoint, 0);
	while (true) {
		asm volatile("nop");
	}
}
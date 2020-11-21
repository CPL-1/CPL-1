c_src := $(shell find ../../src/common/ -type f -name '*.c') $(shell find ../../src/arch/i686/ -type f -name '*.c')
asm_src := $(shell find ../../src/common/ -type f -name '*.s') $(shell find ../../src/arch/i686/ -type f -name '*.s')
c_obj := $(c_src:.c=.o)
asm_obj := $(asm_src:.s=.o)
CC := gcc
AS := nasm
LD := gcc
CFLAGS := -m32 -nostdlib -fno-builtin -fno-stack-protector -mno-red-zone -Wall -Wextra -Werror -c -mno-sse -mno-sse2 -mno-sse3 -mno-mmx -mno-80387 -I../../src -mno-sse4 -mno-sse4.1 -mno-sse4.2 -fno-pic -fomit-frame-pointer -ffreestanding -fstrict-volatile-bitfields -g
ASFLAGS := -f elf
LDFLAGS := -m32 -ffreestanding -static -nostdlib -no-pie

all: build

%.o: %.c
	$(CC) $(CFLAGS) $< -o $@

%.o: %.s
	$(AS) $(ASFLAGS) $< -o $@

build: $(c_obj) $(asm_obj)
	$(LD) -T link.ld $(LDFLAGS) $^ -o kernel.elf -lgcc

.PHONY: clean run run_with_ints debug
clean:
	rm -f $(c_obj)
	rm -f $(asm_obj)

run:
	qemu-system-i386 -kernel kernel.elf -no-reboot -no-shutdown -drive file=../../../test.img,if=none,format=raw,id=NVME1 -device nvme,drive=NVME1,serial=nvme-1 --enable-kvm

run_with_ints:
	qemu-system-i386 -kernel kernel.elf -no-reboot -no-shutdown -drive file=../../../test.img,if=none,format=raw,id=NVME1 -device nvme,drive=NVME1,serial=nvme-1 -singlestep -d int

debug:
	qemu-system-i386 -kernel kernel.elf -d int -no-reboot -no-shutdown -s -S -daemonize -drive file=../../../test.img,if=none,format=raw,id=NVME1 -device nvme,drive=NVME1,serial=nvme-1 -singlestep
	gdb kernel.elf -ex "target remote :1234" -ex "b i686_kernel_main" -ex "c"
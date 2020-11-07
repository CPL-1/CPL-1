c_src := $(shell find src/ -type f -name '*.c')
asm_src := $(shell find src/ -type f -name '*.s')
c_obj := $(c_src:.c=.o)
asm_obj := $(asm_src:.s=.o)
CC := gcc
AS := nasm
LD := gcc
TARGET := i386-elf
CFLAGS := -m32 -nostdlib -fno-builtin -fno-stack-protector -mno-red-zone -Wall -Wextra -O2 -Werror -c -mno-sse -mno-sse2 -mno-sse3 -mno-mmx -mno-80387 -Isrc -lgcc -mno-red-zone -fno-pic -fomit-frame-pointer -ffreestanding -fstrict-volatile-bitfields
ASFLAGS := -f elf -g
LDFLAGS := -m32 -ffreestanding -static -nostdlib -no-pie

all: kernel object_clean run

%.o: %.c
	$(CC) $(CFLAGS) $< -o $@

%.o: %.s
	$(AS) $(ASFLAGS) $< -o $@

kernel: $(c_obj) $(asm_obj)
	$(LD) -T ld/link.ld $(LDFLAGS) $^ -o kernel.elf -lgcc

.PHONY: clean object_clean
object_clean:
	rm -f $(c_obj)
	rm -f $(asm_obj)

clean: object_clean
	rm -f kernel.elf

run: kernel
	qemu-system-x86_64 -kernel kernel.elf -d guest_errors -no-reboot -no-shutdown -drive file=../test.img,if=none,format=raw,id=NVME1 -device nvme,drive=NVME1,serial=nvme-1
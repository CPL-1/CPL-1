c_src := $(shell find src/ -type f -name '*.c')
asm_src := $(shell find src/ -type f -name '*.s')
c_obj := $(c_src:.c=.o)
asm_obj := $(asm_src:.s=.o)
CC := clang
AS := nasm
LD := clang
TARGET := i386-elf
CFLAGS := -nostdlib -fno-builtin -fno-stack-protector -mno-red-zone -Wall -Wextra -g -Og -Werror -c -mno-sse -mno-sse2 -mno-sse3 \
			--target=$(TARGET) -Wno-unused-command-line-argument -Isrc/
ASFLAGS := -f elf -g
LDFLAGS := --target=$(TARGET) -mno-red-zone -Wno-unused-command-line-argument -ffreestanding -O2 -nostdlib -lgcc -fuse-ld=lld -g

all: kernel object_clean run

%.o: %.c
	$(CC) $(CFLAGS) $< -o $@

%.o: %.s
	$(AS) $(ASFLAGS) $< -o $@

kernel: $(c_obj) $(asm_obj)
	$(LD) -T ld/link.ld $(LDFLAGS) $^ -o kernel.elf

.PHONY: clean object_clean
object_clean:
	rm -f $(c_obj)
	rm -f $(asm_obj)

clean: object_clean
	rm -f kernel.elf

run: kernel
	qemu-system-i386 -kernel kernel.elf -d int -no-reboot -no-shutdown

debug: kernel object_clean
	qemu-system-i386 -kernel kernel.elf -d int -no-reboot -no-shutdown -s -S -daemonize
	gdb kernel.elf -ex "target remote :1234" -ex "b kernel_main" -ex "c"
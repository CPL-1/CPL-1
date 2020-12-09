c_src := $(shell find ../../src/common/ -type f -name '*.c') $(shell find ../../src/arch/i686/ -type f -name '*.c')
asm_src := $(shell find ../../src/common/ -type f -name '*.s') $(shell find ../../src/arch/i686/ -type f -name '*.s')
c_obj := $(c_src:.c=.o)
asm_obj := $(asm_src:.s=.o)
CC := i686-elf-gcc
AS := nasm
LD := i686-elf-gcc
CFLAGS := -nostdlib -fno-builtin -fno-stack-protector -mno-red-zone -Wall -Wextra -Werror -c -mno-sse -mno-sse2 -mno-sse3 -mno-mmx -mno-80387 -I../../src -mno-sse4 -mno-sse4.1 -mno-sse4.2 -fno-pic -ffreestanding -fstrict-volatile-bitfields
ASFLAGS := -f elf
LDFLAGS := -ffreestanding -static -nostdlib -no-pie

all: build

%.o: %.c
	$(CC) $(CFLAGS) $< -o $@

%.o: %.s
	$(AS) $(ASFLAGS) $< -o $@

build: $(c_obj) $(asm_obj)
	$(LD) $(LDFLAGS) $^ -o init -lgcc

.PHONY: clean run run_with_ints debug
clean:
	rm -f $(c_obj)
	rm -f $(asm_obj)


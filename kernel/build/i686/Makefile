C_SOURCES := $(shell find ../../src/common/ -type f -name '*.c') $(shell find ../../src/arch/i686/ -type f -name '*.c')
ASM_SOURCES := $(shell find ../../src/common/ -type f -name '*.s') $(shell find ../../src/arch/i686/ -type f -name '*.s')
C_RELEASE_OBJS := $(C_SOURCES:.c=.c.release.o)
C_DEBUG_OBJS := $(C_SOURCES:.c=.c.debug.o)
ASM_OBJS := $(ASM_SOURCES:.s=.s.o)
CC := i686-elf-gcc
AS := nasm
LD := i686-elf-gcc
CFLAGS := -nostdlib -fno-builtin -fno-stack-protector -mno-red-zone -Wall -Wextra -Werror -c -mno-sse -mno-sse2 -mno-sse3 -mno-mmx -mno-80387 -I../../src -mno-sse4 -mno-sse4.1 -mno-sse4.2 -fno-pic -ffreestanding -fstrict-volatile-bitfields
CFLAGS_RELEASE := -O2
CFLAGS_DEBUG := -g
ASFLAGS := -f elf -g
LDFLAGS := -ffreestanding -static -nostdlib -no-pie

%.c.debug.o: %.c
	$(CC) $(CFLAGS) $(CFLAGS_DEBUG) $< -o $@

%.c.release.o: %.c
	$(CC) $(CFLAGS) $(CFLAGS_RELEASE) $< -o $@

%.s.o: %.s
	$(AS) $(ASFLAGS) $< -o $@

debug: $(C_DEBUG_OBJS) $(ASM_OBJS)
	$(LD) -T link.ld $(LDFLAGS) $^ -o kernel-debug.elf -lgcc

release: $(C_RELEASE_OBJS) $(ASM_OBJS)
	$(LD) -T link.ld $(LDFLAGS) $^ -o kernel-release.elf -lgcc

clean:
	rm -f $(C_DEBUG_OBJS)
	rm -f $(C_RELEASE_OBJS)
	rm -f $(ASM_OBJS)
	rm -f kernel-debug.elf
	rm -f kernel-release.elf

.PHONY: clean debug release

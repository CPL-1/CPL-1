C_SOURCES := $(shell find ../../src/common/ -type f -name '*.c') $(shell find ../../src/arch/i686/ -type f -name '*.c')
ASM_SOURCES := $(shell find ../../src/common/ -type f -name '*.s') $(shell find ../../src/arch/i686/ -type f -name '*.s')
C_RELEASE_OBJS := $(C_SOURCES:.c=.c.release.o)
C_DEBUG_OBJS := $(C_SOURCES:.c=.c.debug.o)
ASM_OBJS := $(ASM_SOURCES:.s=.s.o)
CC := i686-elf-gcc
AS := nasm
AR := i686-elf-ar
CFLAGS := -nostdlib -fno-builtin -fno-stack-protector -mno-red-zone -Wall -Wextra -Werror -c -mno-sse -mno-sse2 -mno-sse3 -mno-mmx  -I../../src -I../../include -mno-sse4 -mno-sse4.1 -mno-sse4.2 -fno-pic -ffreestanding -fstrict-volatile-bitfields -g
ASFLAGS := -f elf -g
ARFLAGS := rcs

%.c.debug.o: %.c
	$(CC) $(CFLAGS) $(CFLAGS_DEBUG) $< -o $@

%.c.release.o: %.c
	$(CC) $(CFLAGS) $(CFLAGS_RELEASE) $< -o $@

%.s.o: %.s
	$(AS) $(ASFLAGS) $< -o $@

debug: $(C_DEBUG_OBJS) $(ASM_OBJS)
	$(AR) $(ARFLAGS) libcpl1-debug.a $^

release: $(C_RELEASE_OBJS) $(ASM_OBJS)
	$(AR) $(ARFLAGS) libcpl1-release.a $^

clean:
	rm -f $(C_DEBUG_OBJS)
	rm -f $(C_RELEASE_OBJS)
	rm -f $(ASM_OBJS)
	rm -f libcpl1-debug.a
	rm -f libcpl1-release.a

.PHONY: clean debug release
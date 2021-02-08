C_SOURCES := $(shell find ../../src/ -type f -name '*.c')
C_RELEASE_OBJS := $(C_SOURCES:.c=.c.release.o)
C_DEBUG_OBJS := $(C_SOURCES:.c=.c.debug.o)
CC := i686-elf-gcc
LD := i686-elf-gcc
CFLAGS := -nostdlib -fno-builtin -fno-stack-protector -mno-red-zone -Wall -Wextra -Werror -c -mno-sse -mno-sse2 -mno-sse3 -mno-mmx  -I$(USERLIB_INCLUDE) -mno-sse4 -mno-sse4.1 -mno-sse4.2 -fno-pic -ffreestanding -fstrict-volatile-bitfields -g
LDFLAGS := -ffreestanding -static -nostdlib -no-pie

%.c.debug.o: %.c
	$(CC) $(CFLAGS) $(CFLAGS_DEBUG) $< -o $@

%.c.release.o: %.c
	$(CC) $(CFLAGS) $(CFLAGS_RELEASE) $< -o $@

debug: $(C_DEBUG_OBJS)
	$(LD) $(LDFLAGS) $^ $(USERLIB) -o init-debug -lgcc

release: $(C_RELEASE_OBJS)
	$(LD) $(LDFLAGS) $^ $(USERLIB) -o init-release -lgcc

clean:
	rm -f $(C_DEBUG_OBJS)
	rm -f $(C_RELEASE_OBJS)
	rm -f init-debug
	rm -f init-release

.PHONY: clean debug release

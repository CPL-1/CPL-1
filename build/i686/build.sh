#!/bin/sh

echo "CPL-1 Operating System (for i686 architecture) Build Script"

echo "Building CPL-1 kernel..."
(cd ../../kernel/build/i686 && make clean && make build && cp kernel.elf ../../../build/i686/kernel.elf && rm kernel.elf)
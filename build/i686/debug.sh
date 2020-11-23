#!/bin/bash

qemu-system-i386 -kernel kernel.elf -d int -no-reboot -no-shutdown -s -S -daemonize -drive file=CPL-1_i686.img,if=none,format=raw,id=NVME1 -device nvme,drive=NVME1,serial=nvme-1 -singlestep

gdb kernel.elf -ex "target remote :1234" -ex "b i686_kernel_main" -ex "c"
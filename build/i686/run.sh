#!/bin/bash

qemu-system-i386 -kernel kernel.elf -no-reboot -no-shutdown -drive file=CPL-1_i686.img,if=none,format=raw,id=NVME1 -device nvme,drive=NVME1,serial=nvme-1 --enable-kvm -trace nvme*
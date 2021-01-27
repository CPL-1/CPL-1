.PHONY: build clean run run-with-ints debug testrun testdebug all
all: build obj-clean

../x86/limine/limine-install:
	make -C ../x86/limine limine-install

build: ../x86/limine/limine-install
	cd ../../kernel/build/i686 && make build -j`nproc` && sudo cp kernel.elf ../../../build/i686/kernel.elf
	cd ../../userlib/build/i686 && make build -j`nproc`
	cd ../../userspace/init/build/i686 && make build -j`nproc` && sudo cp init ../../../../build/i686/init
	cd ../../userspace/sh/build/i686 && make build -j`nproc` && sudo cp sh ../../../../build/i686/sh
	cd ../../userspace/ls/build/i686 && make build -j`nproc` && sudo cp ls ../../../../build/i686/ls
	cd ../../userspace/cat/build/i686 && make build -j`nproc` && sudo cp cat ../../../../build/i686/cat
	dd if=/dev/zero bs=1M count=0 seek=64 of=CPL-1_i686.hdd
	dd if=/dev/zero bs=1M count=0 seek=64 of=CPL-1_i686.hdd
	parted -s CPL-1_i686.hdd mklabel gpt
	parted -s CPL-1_i686.hdd mkpart primary 2048s 100%
	sudo losetup -Pf --show CPL-1_i686.hdd > loopback_dev
	sudo partprobe `cat loopback_dev`
	sudo mkdosfs -F32 `cat loopback_dev`p1
	mkdir mount
	sudo mount -tvfat `cat loopback_dev`p1 mount
	sudo cp -R fsroot/* mount
	sudo test -d mount/boot || sudo mkdir mount/boot
	sudo cp limine.cfg mount/boot/limine.cfg
	sudo cp kernel.elf mount/boot/kernel.elf
	sudo cp init mount/sbin/init
	sudo cp sh mount/bin/sh
	sudo cp ls mount/bin/ls
	sudo cp cat mount/bin/cat
	sudo cp -r ../../kernel mount/etc/src/kernel
	sudo cp -r ../../userlib mount/etc/src/userlib
	sudo cp -r ../../userspace mount/etc/src/userspace
	sudo cp -r ../../COPYING mount/etc/src/COPYING
	sudo umount mount
	sudo losetup -d `cat loopback_dev`
	rm -rf mount
	rm -rf loopback_dev
	../x86/limine/limine-install ../x86/limine/limine.bin CPL-1_i686.hdd
	7z a CPL-1_i686.7z CPL-1_i686.hdd

obj-clean:
	cd ../../kernel/build/i686 && make clean
	cd ../../userspace/init/build/i686 && make clean
	cd ../../userspace/sh/build/i686 && make clean
	cd ../../userspace/ls/build/i686 && make clean
	cd ../../userspace/cat/build/i686 && make clean
	cd ../../userlib/build/i686 && make clean
	rm -rf kernel.elf
	rm -rf init
	rm -rf sh
	rm -rf ls

clean: obj-clean
	rm -rf CPL-1_i686.hdd

run: CPL-1_i686.hdd
	qemu-system-i386 -no-reboot -no-shutdown -boot c \
	-drive file=CPL-1_i686.hdd,if=none,format=raw,id=NVME1 \
	-device nvme,drive=NVME1,serial=nvme-1 --enable-kvm -debugcon stdio -m 128M \
	-vga std

run-with-ints: CPL-1_i686.hdd
	qemu-system-i386 -no-reboot -no-shutdown -boot c \
	-drive file=CPL-1_i686.hdd,if=none,format=raw,id=NVME1 \
	-device nvme,drive=NVME1,serial=nvme-1 -debugcon stdio -d int -singlestep -m 128M
	
debug: CPL-1_i686.hdd
	qemu-system-i386 -d int -no-reboot -no-shutdown -boot c \
	-s -S -daemonize -drive file=CPL-1_i686.hdd,if=none,format=raw,id=NVME1 \
	-device nvme,drive=NVME1,serial=nvme-1 -m 128M
	gdb kernel.elf -ex "target remote :1234"

testrun: clean build run
testdebug: clean build debug

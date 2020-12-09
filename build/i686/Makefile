.PHONY: build clean run run-with-ints debug testrun testdebug all
all: clean build

../x86/limine/limine-install:
	make -C ../x86/limine limine-install

CPL-1_i686.hdd: ../x86/limine/limine-install
	cd ../../kernel/build/i686 && make build -j`nproc` && sudo cp kernel.elf ../../../build/i686/kernel.elf
	cd ../../init/build/i686 && make build -j`nproc` && sudo cp init ../../../build/i686/init
	dd if=/dev/zero bs=1M count=0 seek=64 of=CPL-1_i686.hdd
	parted -s CPL-1_i686.hdd mklabel msdos
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
	sudo cp init mount/bin/init
	sudo umount mount
	sudo losetup -d `cat loopback_dev`
	rm -rf mount
	rm -rf loopback_dev
	../x86/limine/limine-install ../x86/limine/limine.bin CPL-1_i686.hdd
	7z a CPL-1_i686.7z CPL-1_i686.hdd

build: CPL-1_i686.hdd

clean:
	cd ../../kernel/build/i686 && make clean
	cd ../../init/build/i686 && make clean
	rm -rf kernel.elf
	rm -rf CPL-1_i686.hdd

run: CPL-1_i686.hdd
	qemu-system-i386 -no-reboot -no-shutdown -boot c \
	-drive file=CPL-1_i686.hdd,if=none,format=raw,id=NVME1 \
	-device nvme,drive=NVME1,serial=nvme-1 --enable-kvm -debugcon stdio

run-with-ints: CPL-1_i686.hdd
	qemu-system-i386 -no-reboot -no-shutdown -boot c \
	-drive file=CPL-1_i686.hdd,if=none,format=raw,id=NVME1 \
	-device nvme,drive=NVME1,serial=nvme-1 -debugcon stdio -d int
	
debug: CPL-1_i686.hdd
	qemu-system-i386 -d int -no-reboot -no-shutdown -boot c \
	-s -S -daemonize -drive file=CPL-1_i686.hdd,if=none,format=raw,id=NVME1 \
	-device nvme,drive=NVME1,serial=nvme-1 -singlestep
	gdb kernel.elf -ex "target remote :1234"

testrun: clean build run
testdebug: clean build debug

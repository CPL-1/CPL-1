.PHONY: build clean run run-with-ints debug testrun testdebug all
all: build obj-clean

../../thirdparty/limine/limine-install:
	cd ../../thirdparty/limine && make limine-install

build: ../../thirdparty/limine/limine-install
	rm -f CPL-1_i686.hdd
	cp limine.cfg ../../out/i686/boot/limine.cfg 2>/dev/null || :
	../../thirdparty/dir2fat32/dir2fat32.sh CPL-1_i686.hdd 64 ../../out/i686
	../../thirdparty/limine/limine-install CPL-1_i686.hdd

clean:
	rm -f CPL-1_i686.hdd
	rm -f CPL-1_i686.7z

run: CPL-1_i686.hdd
	qemu-system-i386 -no-reboot -no-shutdown -boot c \
	-drive file=CPL-1_i686.hdd,if=none,format=raw,id=NVME1 \
	-device nvme,drive=NVME1,serial=nvme-1 --enable-kvm -debugcon stdio -m 32M \
	-vga std

run-with-ints: CPL-1_i686.hdd
	qemu-system-i386 -no-reboot -no-shutdown -boot c \
	-drive file=CPL-1_i686.hdd,if=none,format=raw,id=NVME1 \
	-device nvme,drive=NVME1,serial=nvme-1 -debugcon stdio -d int -singlestep -m 32M
	
debug: CPL-1_i686.hdd
	qemu-system-i386 -d int -no-reboot -no-shutdown -boot c \
	-s -S -daemonize -drive file=CPL-1_i686.hdd,if=none,format=raw,id=NVME1 \
	-device nvme,drive=NVME1,serial=nvme-1 -m 128M
	gdb ../../out/i686/boot/kernel.elf -ex "target remote :1234"

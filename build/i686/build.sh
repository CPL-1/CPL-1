#!/bin/bash

echo "CPL-1 Operating System (for i686 architecture) Build Script"
rm -f CPL-1_i686.img

echo "Building CPL-1 kernel..."
(cd ../../kernel/build/i686 && make build -j$(nproc) && cp kernel.elf ../../../build/i686/kernel.elf && rm kernel.elf)

echo "Creating disk image CPL-1_i686.img..."
chronic dd if=/dev/zero of=CPL-1_i686.img bs=516096c count=1000 2> /dev/null > /dev/null

echo "Formatting disk image with MBR..."
(
echo o # Create a new empty DOS partition table
echo n # Add a new partition
echo   # Partition type (Accept default: primary)
echo 1 # Partition number
echo   # First sector (Accept default)
echo   # Last sector (Accept default)
echo a
echo w # Write changes
) | chronic fdisk -u -C1000 -S63 -H16 CPL-1_i686.img


echo 'Setting up loopback device...'
sudo losetup -o32256 /dev/loop5757 CPL-1_i686.img

echo 'Formatting the partition to FAT32...'
sudo mkdosfs -F32 /dev/loop5757

echo 'Creating mountpoint...'
sudo mkdir /mnt/34cf72b97a0bf2fdc4beb2b8aba703f9

echo 'Mounting FAT32 partition with system data...'
sudo mount -tvfat /dev/loop5757 /mnt/34cf72b97a0bf2fdc4beb2b8aba703f9

echo 'Copying files from /root directory to mounted partition...'
sudo cp -R fsroot/ /mnt/34cf72b97a0bf2fdc4beb2b8aba703f9

echo 'Unmounting loopback device...'
sudo umount /dev/loop5757

echo 'Deleting loopback device...'
sudo losetup -d /dev/loop5757

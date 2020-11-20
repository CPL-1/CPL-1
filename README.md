# CPL-1 Operating System

![Process Test image](screenshots/kernel_init.png)
*CPL-1 kernel init log on i686 CPUs*

### What is CPL-1

CPL-1 Operating System is a hobby operating system project with the goal of creating lightweight and portable operating system that can build itself. It is called like that as the kernel for i686 target runs in ring 1 to avoid variable length interrupt frames (previously CPL-1 was only for i686 processors, so that was quite influential).

### What targets are supported by CPL-1

Currently there is only support for i686 CPUS with PIC 8259 interrupt controller and PCI configuration space access mechianism v1.0

### What I need to build CPL-1?

For i686, CPL-1 uses gcc, libgcc, ld, nasm and GNU Make to build. 

For NixOS, I simply type ```nix-shell -p pkgsi686Linux.gcc pkgsi686Linux.libgcc nasm``` before doing any development

### How I build CPL-1?

#### CPL-1 Kernel

Start with

```
cd kernel/build/<NAME OF ARCH>
```

To build kernel, run
```
make build
```

To delete object files, run
```
make clean
```

To test kernel with qemu, run
```
make run
```

To debug kernel, run
```
make debug
```

### What CPL-1 is licensed under?

CPL-1 uses MIT license. In short, it means that you need to cite this codebase if you are planning to use code from this repository. Don't quote me on this though.
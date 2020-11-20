ENTRY(i686_mb1_start)

SECTIONS
{
    . = 1M;
    .inittext : ALIGN(4K)
    {
        *(.multiboot)
        *(.inittext)
    }

    . += 0xC0000000;

    .text ALIGN(4K) : AT(ADDR (.text) - 0xC0000000)
    {
        *(.text)
    }

    .rodata ALIGN(4K) : AT(ADDR (.rodata) - 0xC0000000)
    {
        *(.rodata)
    }

    .data ALIGN(4K) : AT(ADDR (.data) - 0xC0000000)
    {
        *(.data)
    }

    .bss ALIGN(4K) : AT(ADDR (.bss) - 0xC0000000)
    {
        *(.bss)
    }

    i686_phys_kernel_end = . - 0xC0000000;
}
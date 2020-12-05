ENTRY(i686_Stivale_EntryPoint)

SECTIONS
{
    . = 1M;
    .stivalehdr : ALIGN(4K) 
    {
        *(.stivalehdr)
    }
    
    .inittext : ALIGN(4K)
    {
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

    i686_PhysicalMM_KernelEnd = . - 0xC0000000;
}
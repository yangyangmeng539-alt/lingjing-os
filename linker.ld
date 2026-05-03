ENTRY(start)

SECTIONS
{
    . = 1M;

    start = .;

    .text BLOCK(4K) : ALIGN(4K)
    {
        *(.multiboot)
        *(.multiboot2)
        *(.text)
    }

    .rodata BLOCK(4K) : ALIGN(4K)
    {
        *(.rodata)
    }

    .data BLOCK(4K) : ALIGN(4K)
    {
        *(.data)
    }

    .bss BLOCK(4K) : ALIGN(4K)
    {
        *(COMMON)
        *(.bss)
    }

    end = .;
}
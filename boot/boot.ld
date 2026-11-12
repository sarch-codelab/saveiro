OUTPUT_FORMAT(elf32-i386)
ENTRY(_start)
SECTIONS
{
    . = 0x7C00;
    .text : {
        *(.text)
        *(.rodata*)
        *(.data*)
    }
    . = 0x7DFE;
    .sig : {
        SHORT(0xAA55);
    }
}

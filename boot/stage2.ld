OUTPUT_FORMAT(elf32-i386)
ENTRY(_stage2)
SECTIONS
{
    . = 0x7E00;
    .text : {
        *(.text)
        *(.rodata*)
        *(.data*)
    }
    /DISCARD/ : { *(.comment) *(.note*) *(.eh_frame*) }
}

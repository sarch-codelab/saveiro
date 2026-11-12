/* SaveiroOS - punto de entrada del kernel (32 bits) */

.section .text.entry
.global _start
.extern kmain

_start:
    cli
    movl    4(%esp), %esi       /* boot_info que dejó la etapa 2 */
    movl    $0x90000, %esp
    xorl    %ebp, %ebp

    /* Poner a cero .bss */
    movl    $__bss_start, %edi
    movl    $__bss_end, %ecx
    subl    %edi, %ecx
    xorl    %eax, %eax
    rep stosb

    pushl   %esi
    call    kmain

1:  cli
    hlt
    jmp     1b

.section .note.GNU-stack,"",@progbits

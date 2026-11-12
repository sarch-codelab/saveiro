/* SaveiroOS - segunda etapa del cargador
 *
 * Cargada en 0x7E00 por el sector de arranque. Se encarga de:
 *   1. habilitar la línea A20
 *   2. obtener el mapa de memoria del BIOS (E820) en 0x5000
 *   3. cargar el kernel desde el disquete a 0x10000
 *   4. pasar a modo protegido de 32 bits y saltar al kernel
 */
.code16
.section .text
.global _stage2

_stage2:
    cli
    xorw    %ax, %ax
    movw    %ax, %ds
    movw    %ax, %es
    movw    %ax, %ss
    movw    $0x7C00, %sp
    sti
    movb    %dl, drive

    movw    $msg_s2, %si
    call    puts

    /* --- A20 por el puerto 0x92 (fast gate) --- */
    inb     $0x92, %al
    orb     $0x02, %al
    andb    $0xFE, %al
    outb    %al, $0x92

    /* --- mapa de memoria E820 --- */
    movl    $0, 0x5000           /* número de entradas */
    movw    $0x5004, %di
    xorl    %ebx, %ebx
e820_loop:
    movl    $0xE820, %eax
    movl    $0x534D4150, %edx
    movl    $24, %ecx
    int     $0x15
    jc      e820_done
    cmpl    $0x534D4150, %eax
    jne     e820_done
    cmpl    $20, %ecx
    jb      e820_next
    cmpl    $48, 0x5000
    jae     e820_done
    incl    0x5000
    addw    $24, %di
e820_next:
    testl   %ebx, %ebx
    jz      e820_done
    jmp     e820_loop
e820_done:
    movb    $'.', %al
    call    putc

    /* --- cargar el kernel --- */
    movw    sects+8, %cx
    testw   %cx, %cx
    jz      load_fail
    movl    $5, lba              /* el kernel empieza en el LBA 5 */
    movl    $0x10000, dest
read_loop:
    movw    %cx, save_cx
    call    read_sector
    movw    save_cx, %cx
    incl    lba
    addl    $512, dest
    decw    %cx
    jnz     read_loop

    movb    $'.', %al
    call    putc

    /* --- modo protegido --- */
    cli
    lgdt    gdt_desc
    movl    %cr0, %eax
    orl     $1, %eax
    movl    %eax, %cr0
    .byte   0x66, 0xEA           /* jmp far 0x08:pm32 */
    .long   pm32
    .word   0x08

.code32
pm32:
    movw    $0x10, %ax
    movw    %ax, %ds
    movw    %ax, %es
    movw    %ax, %fs
    movw    %ax, %gs
    movw    %ax, %ss
    movl    $0x90000, %esp
    xorl    %ebp, %ebp

    pushl   $0x5000              /* puntero al boot_info */
    movl    $0x10000, %eax
    call    *%eax                /* kmain nunca regresa */
    cli
1:  hlt
    jmp     1b

.code16
/* Lee un sector: LBA en 'lba', destino físico en 'dest' */
read_sector:
    movl    lba, %eax
    xorl    %edx, %edx
    movl    $18, %ebx
    divl    %ebx                 /* eax = lba/18, edx = lba%18 */
    incb    %dl
    movb    %dl, sec
    xorl    %edx, %edx
    movl    $2, %ebx
    divl    %ebx                 /* eax = cilindro, edx = cabeza */
    movb    %dl, head
    movb    %al, cyl

    movl    dest, %eax
    movl    %eax, %edx
    andl    $0x0F, %edx
    shrl    $4, %eax
    movw    %ax, %es
    movw    %dx, %bx

    movb    $3, retries
1:
    movb    drive, %dl
    movb    head, %dh
    movb    cyl, %ch
    movb    sec, %cl
    movw    $0x0201, %ax
    int     $0x13
    jnc     2f
    xorb    %ah, %ah
    movb    drive, %dl
    int     $0x13
    decb    retries
    jnz     1b
    jmp     load_fail
2:
    ret

puts:
    lodsb
    testb   %al, %al
    jz      2f
    movb    $0x0E, %ah
    movw    $0x0007, %bx
    int     $0x10
    jmp     puts
2:  ret

putc:
    movb    $0x0E, %ah
    movw    $0x0007, %bx
    int     $0x10
    ret

load_fail:
    movw    $msg_err, %si
    call    puts
1:  hlt
    jmp     1b

.align 8
gdt:
    .quad   0x0000000000000000
    .quad   0x00CF9A000000FFFF   /* código: base 0, límite 4 GiB, 32 bits */
    .quad   0x00CF92000000FFFF   /* datos  */
gdt_desc:
    .word   gdt_desc - gdt - 1
    .long   gdt

.align 2
sects:
    .ascii  "SVRSECTS"
    .word   0

drive:      .byte 0
lba:        .long 0
dest:       .long 0
sec:        .byte 0
head:       .byte 0
cyl:        .byte 0
retries:    .byte 0
save_cx:    .word 0

msg_s2:     .asciz "etapa 2"
msg_err:    .asciz " error al leer el kernel\r\n"

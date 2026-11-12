/* SaveiroOS - sector de arranque (512 bytes, modo real 16 bits)
 *
 * El BIOS carga este sector en 0x7C00 y salta a él con DL = unidad de
 * arranque. Nuestra única misión aquí es cargar la segunda etapa (4 sectores
 * en 0x7E00) usando la BIOS y saltar a ella.
 */
.code16
.section .text
.global _start

_start:
    cli
    xorw    %ax, %ax
    movw    %ax, %ds
    movw    %ax, %es
    movw    %ax, %ss
    movw    $0x7C00, %sp
    sti

    movb    %dl, drive

    movw    $msg_boot, %si
    call    puts

    /* Reiniciar el controlador de disco */
    movb    drive, %dl
    xorb    %ah, %ah
    int     $0x13
    jc      fail

    /* Leer 4 sectores (2 KiB) a partir del sector 2 -> 0x7E00 */
    movw    $0x0204, %ax
    movb    $0, %ch              /* cilindro 0 */
    movb    $2, %cl              /* sector 2 */
    movb    $0, %dh              /* cabeza 0 */
    movb    drive, %dl
    movw    $0x7E00, %bx
    int     $0x13
    jc      fail

    movw    $msg_ok, %si
    call    puts

    /* Salto lejano a 0000:7E00 */
    pushw   $0
    pushw   $0x7E00
    retf

fail:
    movw    $msg_fail, %si
    call    puts
1:  hlt
    jmp     1b

/* Imprime la cadena terminada en NUL apuntada por SI (BIOS teletipo) */
puts:
    lodsb
    testb   %al, %al
    jz      2f
    movb    $0x0E, %ah
    movw    $0x0007, %bx
    int     $0x10
    jmp     puts
2:  ret

drive:      .byte 0
msg_boot:   .asciz "SaveiroOS boot\r\n"
msg_ok:     .asciz "etapa 1 ok\r\n"
msg_fail:   .asciz "error de disco\r\n"

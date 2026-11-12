#!/usr/bin/env python3
"""Construye la imagen de disquete de 1.44 MB de SaveiroOS.

Distribución:
    sector 0        : boot.bin (512 bytes, firma 0xAA55)
    sectores 1..4   : stage2.bin (2 KiB)
    sector 5 en adelante : kernel.bin

El número de sectores del kernel se escribe en el marcador "SVRSECTS"
de la segunda etapa.
"""

import os
import struct
import sys

SECTOR = 512
STAGE2_SECTORS = 4
KERNEL_LBA = 1 + STAGE2_SECTORS
IMAGE_SIZE = 1474560  # disquete de 1.44 MB


def read(path):
    with open(path, "rb") as f:
        return f.read()


def main():
    if len(sys.argv) != 5:
        print("uso: mkimage.py <boot.bin> <stage2.bin> <kernel.bin> <salida.img>")
        return 1

    boot, stage2, kernel, out = sys.argv[1:]

    boot = read(boot)
    stage2 = read(stage2)
    kernel = read(kernel)

    if len(boot) != SECTOR:
        raise SystemExit(f"boot.bin debe medir 512 bytes, mide {len(boot)}")

    if boot[510:512] != b"\x55\xaa":
        raise SystemExit("falta la firma de arranque 0xAA55")

    if len(stage2) > STAGE2_SECTORS * SECTOR:
        raise SystemExit("stage2 no cabe en 4 sectores")

    kernel_sectors = (len(kernel) + SECTOR - 1) // SECTOR

    marker = b"SVRSECTS"
    pos = stage2.find(marker)
    if pos < 0:
        raise SystemExit("no se encontró el marcador SVRSECTS en stage2")

    stage2 = bytearray(stage2)
    struct.pack_into("<H", stage2, pos + len(marker), kernel_sectors)
    stage2 = bytes(stage2)

    image = bytearray()
    image += boot
    image += stage2 + b"\x00" * (STAGE2_SECTORS * SECTOR - len(stage2))
    image += kernel + b"\x00" * (kernel_sectors * SECTOR - len(kernel))
    image += b"\x00" * (IMAGE_SIZE - len(image))

    with open(out, "wb") as f:
        f.write(image)

    print(f"  boot    : {len(boot):>6} bytes")
    print(f"  stage2  : {len(stage2):>6} bytes ({STAGE2_SECTORS} sectores)")
    print(f"  kernel  : {len(kernel):>6} bytes ({kernel_sectors} sectores, LBA {KERNEL_LBA})")
    print(f"  imagen  : {out} ({len(image)} bytes)")
    return 0


if __name__ == "__main__":
    sys.exit(main())

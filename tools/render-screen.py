#!/usr/bin/env python3
"""Convierte una captura JSON de la memoria VGA en un PNG con la paleta VGA."""

import json
import sys
from PIL import Image, ImageDraw, ImageFont

CELL_W = 8
CELL_H = 16
COLS = 80
ROWS = 25
SCALE = 2

PALETTE = [
    (0x00, 0x00, 0x00), (0x00, 0x00, 0xAA), (0x00, 0xAA, 0x00), (0x00, 0xAA, 0xAA),
    (0xAA, 0x00, 0x00), (0xAA, 0x00, 0xAA), (0xAA, 0x55, 0x00), (0xAA, 0xAA, 0xAA),
    (0x55, 0x55, 0x55), (0x55, 0x55, 0xFF), (0x55, 0xFF, 0x55), (0x55, 0xFF, 0xFF),
    (0xFF, 0x55, 0x55), (0xFF, 0x55, 0xFF), (0xFF, 0xFF, 0x55), (0xFF, 0xFF, 0xFF),
]


def main():
    if len(sys.argv) < 3:
        print("uso: render-screen.py <captura.json> <salida.png>")
        return 1

    with open(sys.argv[1], "r", encoding="utf-8") as handle:
        rows = json.load(handle)

    image = Image.new("RGB", (COLS * CELL_W, ROWS * CELL_H), PALETTE[0])
    draw = ImageDraw.Draw(image)

    font = ImageFont.truetype(
        "/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf", 14
    )

    # Primero todos los fondos: así los trazos que bajan de una celda
    # (guiones bajos, comas) no los tapa la fila siguiente.
    for row in range(ROWS):
        for col in range(COLS):
            attribute = rows[row][col][1]
            bg = PALETTE[(attribute >> 4) & 0x0F]
            x = col * CELL_W
            y = row * CELL_H
            draw.rectangle([x, y, x + CELL_W - 1, y + CELL_H - 1], fill=bg)

    for row in range(ROWS):
        for col in range(COLS):
            char_code, attribute = rows[row][col]
            fg = PALETTE[attribute & 0x0F]

            if char_code and char_code >= 32:
                x = col * CELL_W
                y = row * CELL_H
                draw.text((x, y), chr(char_code), font=font, fill=fg)

    image = image.resize(
        (image.width * SCALE, image.height * SCALE), Image.Resampling.NEAREST
    )
    image.save(sys.argv[2])
    print(f"imagen guardada en {sys.argv[2]} ({image.width}x{image.height})")
    return 0


if __name__ == "__main__":
    sys.exit(main())

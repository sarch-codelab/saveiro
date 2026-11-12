#!/usr/bin/env python3
"""Convierte un volcado del framebuffer SVGA (8 bpp) a PNG.

Uso: render-framebuffer.py <captura.raw> <paleta.json> <salida.png> [escala]
"""

import json
import sys

from PIL import Image


def main():
    if len(sys.argv) < 4:
        print("uso: render-framebuffer.py <captura.raw> <paleta.json> <salida.png> [escala]")
        return 1

    raw = open(sys.argv[1], "rb").read()
    palette = json.load(open(sys.argv[2], "r", encoding="utf-8"))
    scale = int(sys.argv[4]) if len(sys.argv) > 4 else 1

    size = 800 * 600
    if len(raw) < size:
        print(f"el volcado tiene {len(raw)} bytes, se esperaban {size}")
        return 1

    colors = [
        ((entry >> 16) & 0xFF, (entry >> 8) & 0xFF, entry & 0xFF)
        for entry in palette
    ]

    image = Image.new("RGB", (800, 600))
    image.putdata([colors[value] for value in raw[:size]])

    if scale != 1:
        image = image.resize((image.width * scale, image.height * scale), Image.NEAREST)

    image.save(sys.argv[3])
    print(f"escrito {sys.argv[3]}")
    return 0


if __name__ == "__main__":
    sys.exit(main())

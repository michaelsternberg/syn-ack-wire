#!/usr/bin/env python3
"""Draw an MS-DOS screen dump as an IBM monochrome display shows it, for the manual's captures.

Usage:
    render_msdos.py ROM DUMP OUT.png [--text]

ROM is the IBM MDA/CGA character ROM (5788005.u33, 8 KB), or a MAME zip holding it (isa_ibm_mda.zip,
cga.zip); it is not part of the repository. DUMP is the 40x24 game area as character and attribute bytes,
1920 bytes, as the DOS autoplay build and the PC capture build save it (test/dos_dump.c, make pc-msdos).
--text prints the dump as text instead (code page 437; reverse letters in lowercase).

The font is the MDA's, 9 x 14: 8 columns from the ROM, the ninth a copy of the eighth for the line drawing
characters C0-DF, as the MDA does. A font pixel is drawn 4 image pixels wide and 3 tall, so a capture has
the proportions of the other manuals' (a PC's monitor draws its text taller still). Attributes become greys:
normal light grey, bright white, dark grey, reverse black on light grey; a blinking cell is drawn in its
reverse state. Geometry, for \\capgeom in the manual's
platform.tex: the text starts at (36, 21), a cell is 36 x 42, the image 1512 x 1050."""
import sys
import zipfile
from PIL import Image

SX, SY = 4, 3                   # image pixels a font pixel
BX, BY = 9, 7                   # border, in font pixels
GREY = {0: (0, 0, 0), 7: (0xAA, 0xAA, 0xAA), 8: (0x55, 0x55, 0x55), 15: (0xFF, 0xFF, 0xFF)}


def load_rom(path):
    if path.endswith(".zip"):
        z = zipfile.ZipFile(path)
        return z.read("5788005.u33")
    return open(path, "rb").read()


def glyph(rom, c):
    """The MDA's 14 rows of character c, each 9 bits wide (bit 8 the left column)."""
    rows = [rom[c * 8 + y] for y in range(8)] + [rom[0x800 + c * 8 + y] for y in range(6)]
    return [r << 1 | (r & 1 if 0xC0 <= c <= 0xDF else 0) for r in rows]


def colours(attr):
    fg, bg = attr & 0x0F, attr >> 4 & 7
    if bg:                                  # reverse: dark on light
        return GREY[0], GREY[7]
    return GREY.get(fg, GREY[7]), GREY[0]


def render(rom, dump):
    w, h = 40 * 9 + 2 * BX, 24 * 14 + 2 * BY
    img = Image.new("RGB", (w, h), GREY[0])
    px = img.load()
    for i in range(40 * 24):
        c, a = dump[2 * i], dump[2 * i + 1]
        fore, back = colours(a)
        g = glyph(rom, c)
        x0, y0 = BX + i % 40 * 9, BY + i // 40 * 14
        for y in range(14):
            for x in range(9):
                px[x0 + x, y0 + y] = fore if g[y] >> (8 - x) & 1 else back
    return img.resize((w * SX, h * SY), Image.NEAREST)


def text(dump):
    lines = []
    for r in range(24):
        line = ""
        for i in range(r * 40, r * 40 + 40):
            c, a = dump[2 * i], dump[2 * i + 1]
            ch = bytes([c]).decode("cp437") if c >= 0x20 else " "
            line += ch.lower() if a & 0x70 and ch.isalpha() else ch
        lines.append(line)
    return lines


def main():
    args = sys.argv[1:]
    if len(args) < 3:
        sys.exit(__doc__)
    dump = open(args[1], "rb").read()
    if len(dump) != 1920:
        sys.exit("%s: %d bytes, a dump is 1920" % (args[1], len(dump)))
    if "--text" in args:
        print("\n".join(text(dump)))
        return
    render(load_rom(args[0]), dump).save(args[2])


if __name__ == "__main__":
    main()

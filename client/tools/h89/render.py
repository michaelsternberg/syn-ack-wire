#!/usr/bin/env python3
"""Draw an H89 screen dump as the H19 terminal shows it, for the manual's captures.

Usage:
    render.py ROM DUMP OUT.png       the picture
    render.py --text DUMP            the game's 40 columns as text (reverse letters in lowercase)

ROM is the H19's character ROM (2716_444-29_h19font.u420, 2 KB), or MAME's heath_tlb.zip holding it; it is
not part of the repository. DUMP is the whole 80x24 screen, each cell a character then its attributes (1
reverse video, 2 the graphics characters), as the PC capture build saves it (make pc-h89).

The ROM holds 128 glyphs of 16 bytes, 8 dots wide and 10 rows used. In graphics mode the codes 60h-7Eh are
its first 31 glyphs, 5Fh the 32nd, and 5Eh its last (the dot); the other codes are the same either way. A
font dot is drawn 2 image pixels wide and 4 tall, as the H19's tube draws its 640 x 240 dots in a 4:3
screen; white on black, reversed for reverse video. Geometry, for \\capgeom in the manual's platform.tex:
the screen starts at (32, 24), a cell is 16 x 40, the image 1344 x 1008; the game's 40 columns start at
column 20, at x = 352."""
import sys
import zipfile
from PIL import Image

SX, SY = 2, 4                   # image pixels a font dot
BX, BY = 32, 24                 # border, in image pixels
COLS, ROWS = 80, 24
INV, GFX = 1, 2
WHITE, BLACK = (0xE8, 0xE8, 0xE8), (0x0C, 0x0C, 0x0C)


def load_rom(path):
    if path.endswith(".zip"):
        return zipfile.ZipFile(path).read("2716_444-29_h19font.u420")
    return open(path, "rb").read()


def glyph_index(c, a):
    if a & GFX and c >= 0x5E:
        return 0x7F if c == 0x5E else 0x1F if c == 0x5F else c - 0x60
    return c & 0x7F


def render(rom, dump, out):
    w, h = 8 * SX, 10 * SY
    img = Image.new("RGB", (2 * BX + COLS * w, 2 * BY + ROWS * h), BLACK)
    px = img.load()
    for k in range(COLS * ROWS):
        c, a = dump[2 * k], dump[2 * k + 1]
        g = rom[glyph_index(c, a) * 16:][:10]
        x0, y0 = BX + (k % COLS) * w, BY + (k // COLS) * h
        for y in range(10):
            for x in range(8):
                on = bool(g[y] >> (7 - x) & 1) != bool(a & INV)
                if on:
                    for dy in range(SY):
                        for dx in range(SX):
                            px[x0 + x * SX + dx, y0 + y * SY + dy] = WHITE
    img.save(out)


def text(dump):
    """The game's 40 columns (20-59), 24 lines: what tools/capture.sh looks for."""
    lines = []
    for r in range(ROWS):
        s = ""
        for c in range(20, 60):
            k = r * COLS + c
            ch, a = chr(dump[2 * k]), dump[2 * k + 1]
            if a & GFX and ch >= "^":
                ch = "#"                # a piece of the board: nothing to read
            s += ch.lower() if a & INV else ch
        lines.append(s)
    return lines


def main():
    if sys.argv[1:2] == ["--text"]:
        print("\n".join(text(open(sys.argv[2], "rb").read())))
        return
    if len(sys.argv) != 4:
        sys.exit(__doc__)
    render(load_rom(sys.argv[1]), open(sys.argv[2], "rb").read(), sys.argv[3])


if __name__ == "__main__":
    main()

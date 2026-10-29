#!/usr/bin/env python3
"""Draw an NES screen dump as the screen looks, for the manual's captures and for checking.

Usage:
    render_nes.py DUMP OUT.png [--scale N]          a dump: make pc-nes's, or the autoplay build's (tools/nes.lua)
    render_nes.py --text DUMP                       the same as text
    render_nes.py --page HELPnn.TXT [--qr URL] OUT.png    an instruction page, as src/nes/help.c draws it

A dump is 1280 bytes: the game's name table as txt_cells holds it (960: 30 rows of 32 tiles), its attribute
table (64) and the sprites (256, OAM). It is drawn with the tiles src/nes/text.c makes (font_nes.tiles()), the
palettes and sprite tiles from text.c itself, and FCEUX's NTSC colours; only the 224 lines a TV shows (name
table rows 1-28). A title screen's static is a still of it."""
import argparse
import os
import random
import re

from PIL import Image

import font_nes
import help_nes

HERE = os.path.dirname(os.path.abspath(__file__))
SRC = os.path.join(HERE, "..", "src", "nes")
FONT = os.path.join(SRC, "font.txt")

# FCEUX's palette
NES_RGB = [
    (0x74, 0x74, 0x74), (0x24, 0x18, 0x8C), (0x00, 0x00, 0xA8), (0x44, 0x00, 0x9C), (0x8C, 0x00, 0x74),
    (0xA8, 0x00, 0x10), (0xA4, 0x00, 0x00), (0x7C, 0x08, 0x00), (0x40, 0x2C, 0x00), (0x00, 0x44, 0x00),
    (0x00, 0x50, 0x00), (0x00, 0x3C, 0x14), (0x18, 0x3C, 0x5C), (0x00, 0x00, 0x00), (0x00, 0x00, 0x00),
    (0x00, 0x00, 0x00), (0xBC, 0xBC, 0xBC), (0x00, 0x70, 0xEC), (0x20, 0x38, 0xEC), (0x80, 0x00, 0xF0),
    (0xBC, 0x00, 0xBC), (0xE4, 0x00, 0x58), (0xD8, 0x28, 0x00), (0xC8, 0x4C, 0x0C), (0x88, 0x70, 0x00),
    (0x00, 0x94, 0x00), (0x00, 0xA8, 0x00), (0x00, 0x90, 0x38), (0x00, 0x80, 0x88), (0x00, 0x00, 0x00),
    (0x00, 0x00, 0x00), (0x00, 0x00, 0x00), (0xFC, 0xFC, 0xFC), (0x3C, 0xBC, 0xFC), (0x5C, 0x94, 0xFC),
    (0xCC, 0x88, 0xFC), (0xF4, 0x78, 0xFC), (0xFC, 0x74, 0xB4), (0xFC, 0x74, 0x60), (0xFC, 0x98, 0x38),
    (0xF0, 0xBC, 0x3C), (0x80, 0xD0, 0x10), (0x4C, 0xDC, 0x48), (0x58, 0xF8, 0x98), (0x00, 0xE8, 0xD8),
    (0x78, 0x78, 0x78), (0x00, 0x00, 0x00), (0x00, 0x00, 0x00), (0xFC, 0xFC, 0xFC), (0xA8, 0xE4, 0xFC),
    (0xC4, 0xD4, 0xFC), (0xD4, 0xC8, 0xFC), (0xFC, 0xC4, 0xFC), (0xFC, 0xC4, 0xD8), (0xFC, 0xBC, 0xB0),
    (0xFC, 0xD8, 0xA8), (0xFC, 0xE4, 0xA0), (0xE0, 0xFC, 0xA0), (0xA8, 0xF0, 0xBC), (0xB0, 0xFC, 0xCC),
    (0x9C, 0xFC, 0xF0), (0xC4, 0xC4, 0xC4), (0x00, 0x00, 0x00), (0x00, 0x00, 0x00),
]
ROW0, ROWS = 1, 28          # what a TV shows: name table rows 1-28


def c_array(name):
    """The bytes of text.c's array `name`."""
    src = open(os.path.join(SRC, "text.c")).read()
    body = re.search(r"%s\[\d+\]\s*=\s*\{(.*?)\};" % name, src, re.S).group(1)
    body = re.sub(r"/\*.*?\*/", "", body, flags=re.S)
    return [int(v, 16) for v in re.findall(r"0x[0-9A-Fa-f]+", body)]


def text(data):
    """A dump's 30 name table rows as text, for tools/capture.sh: inverse dropped, the yen sign as \\, the logo's
    lines and static as # and :."""
    def ch(b):
        c = b & 0x7F
        return chr(c) if 0x20 <= c < 0x7F else "#" if c < 0x0C else ":" if c < 0x10 else " "
    return ["".join(ch(b) for b in data[r * 32:r * 32 + 32]) for r in range(30)]


def draw(cells, attrs, oam):
    tiles = font_nes.tiles(FONT)
    pal = c_array("palettes")
    spr = c_array("sprites")
    rnd = random.Random(1)
    for t in range(font_nes.NOISE, font_nes.NOISE + 4):
        tiles[t] = [[rnd.randrange(4) for _ in range(8)] for _ in range(8)]
    im = Image.new("RGB", (256, 240))
    px = im.load()
    for r in range(30):
        for c in range(32):
            a = attrs[(r >> 2) * 8 + (c >> 2)] >> (((r >> 1) & 1) * 4 + ((c >> 1) & 1) * 2) & 3
            tile = tiles[cells[r * 32 + c]]
            for y in range(8):
                for x in range(8):
                    v = tile[y][x]
                    px[c * 8 + x, r * 8 + y] = NES_RGB[pal[0] if v == 0 else pal[a * 4 + v]]
    for k in range(64):
        y, t, a, x = oam[4 * k:4 * k + 4]
        if y >= 0xEF or not 1 <= t <= len(spr) // 8:
            continue
        for dy in range(8):
            bits = spr[(t - 1) * 8 + dy]
            for dx in range(8):
                if bits >> (7 - dx) & 1 and x + dx < 256 and y + 1 + dy < 240:
                    px[x + dx, y + 1 + dy] = NES_RGB[pal[16 + (a & 3) * 4 + 1]]
    return im.crop((0, ROW0 * 8, 256, (ROW0 + ROWS) * 8))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("dump", nargs="?")
    ap.add_argument("out", nargs="?")
    ap.add_argument("--scale", type=int, default=3)
    ap.add_argument("--text", action="store_true")
    ap.add_argument("--page")
    ap.add_argument("--qr")
    a = ap.parse_args()
    if a.page:
        grid, _ = help_nes.page_cells(a.page, a.qr, quiet=True)
        cells, attrs, oam, out = [b for row in grid for b in row], [0] * 64, [0xF8] * 256, a.dump
    else:
        data = open(a.dump, "rb").read()
        if len(data) != 1280:
            raise SystemExit("%s: %d bytes, a dump is 1280" % (a.dump, len(data)))
        if a.text:
            print("\n".join(text(data)))
            return
        cells, attrs, oam, out = data[:960], data[960:1024], data[1024:], a.out
    im = draw(cells, attrs, oam)
    im.resize((im.width * a.scale, im.height * a.scale), Image.NEAREST).save(out)


if __name__ == "__main__":
    main()

#!/usr/bin/env python3
"""Draw an Adam screen dump as the screen looks, for the manual's captures and for checking.

Usage:
    render_adam.py DUMP OUT.png [--scale N]         a dump: make pc-adam's
    render_adam.py --text DUMP                      the same as text
    render_adam.py --page HELPnn.TXT [--qr URL] OUT.png    an instruction page, as src/adam/help.c draws it

A dump is 1560 bytes: the 768 cells as txt_cells holds them (24 rows of 32), their palettes (txt_pal, 768) and
the sprites' attributes (24: 6 of y, x, pattern, colour). It is drawn as src/adam/text.c paints it: the glyphs
from src/nes/font.txt, the palettes and sprite patterns from text.c itself, and the TMS9918A's colours worked out
from its datasheet as FujiNet Go Adam's core does (adamcore's palette 0). A title screen's static is a still of it."""
import argparse
import os
import random
import re

from PIL import Image

import font_nes
import help_nes

HERE = os.path.dirname(os.path.abspath(__file__))
SRC = os.path.join(HERE, "..", "src", "adam")
FONT = os.path.join(HERE, "..", "src", "nes", "font.txt")

# the TMS9918A's colours 1-15 as (Y, R-Y, B-Y) from its datasheet, 0.47 no excursion
TMS_YUV = [(0.00, 0.47, 0.47), (0.53, 0.07, 0.20), (0.67, 0.17, 0.27), (0.40, 0.40, 1.00), (0.53, 0.43, 0.93),
           (0.47, 0.83, 0.30), (0.73, 0.00, 0.70), (0.53, 0.93, 0.27), (0.67, 0.93, 0.27), (0.73, 0.57, 0.07),
           (0.80, 0.57, 0.17), (0.47, 0.13, 0.23), (0.53, 0.73, 0.67), (0.80, 0.47, 0.47), (1.00, 0.47, 0.47)]
NOISE, QR, INV = 0x0C, 0x10, 0x80
WHITE = 0xF1                    # text.c COL_WHITE: the static, the quarter blocks


def rgb(n):
    if n == 0:
        return (0, 0, 0)
    y, ry, by = TMS_YUV[n - 1]
    r, b = y + ry - 0.47, y + by - 0.47
    g = (y - 0.30 * r - 0.11 * b) / 0.59
    return tuple(int(min(1, max(0, v)) * 255 + 0.5) for v in (r, g, b))


def c_array(name):
    """The bytes of text.c's array `name`."""
    src = open(os.path.join(SRC, "text.c")).read()
    body = re.search(r"%s\[\d+\]\s*=\s*\{(.*?)\};" % name, src, re.S).group(1)
    body = re.sub(r"/\*.*?\*/", "", body, flags=re.S)
    return [int(v, 16) for v in re.findall(r"0x[0-9A-Fa-f]+", body)]


def patterns():
    """Each cell code's 8 rows of 8 bits, as text.c's pattern() gives them; the static a still."""
    pat = {}
    for code, _, rows in font_nes.parse(FONT):
        pat[code] = font_nes.bits(rows)
    for n in range(16):
        top = (0xF0 if n & 8 else 0) | (0x0F if n & 4 else 0)
        bottom = (0xF0 if n & 2 else 0) | (0x0F if n & 1 else 0)
        pat[QR + n] = [top] * 4 + [bottom] * 4
    rnd = random.Random(1)
    for t in range(NOISE, NOISE + 4):
        pat[t] = [rnd.randrange(256) for _ in range(8)]
    return pat


def text(data):
    """A dump's 24 rows as text, for tools/capture.sh: inverse dropped, the yen sign as \\, the logo's lines and
    static as # and :."""
    def ch(b):
        c = b & 0x7F
        return chr(c) if 0x20 <= c < 0x7F else "#" if c < NOISE else ":" if c < QR else " "
    return ["".join(ch(b) for b in data[r * 32:r * 32 + 32]) for r in range(24)]


def draw(cells, pals, sat):
    pat = patterns()
    colours = c_array("colours")
    spr = c_array("sprites")
    im = Image.new("RGB", (256, 192))
    px = im.load()
    for i, g in enumerate(cells):
        r, c = divmod(i, 32)
        if NOISE <= g & 0x7F < QR + 16:
            col = WHITE
        else:
            col = colours[pals[i]]
            if g & INV:
                col = (col << 4 | col >> 4) & 0xFF
        fg, bg = rgb(col >> 4), rgb(col & 15)
        rows = pat.get(g & 0x7F, [0] * 8)
        for y in range(8):
            for x in range(8):
                px[c * 8 + x, r * 8 + y] = fg if rows[y] >> (7 - x) & 1 else bg
    for k in range(len(sat) // 4):
        y, x, t, a = sat[4 * k:4 * k + 4]
        if y == 0xD0:
            break
        if y >= 0xC0 or t >= len(spr) // 8:
            continue
        for dy in range(8):
            bits = spr[t * 8 + dy]
            for dx in range(8):
                if bits >> (7 - dx) & 1 and x + dx < 256 and y + 1 + dy < 192:
                    px[x + dx, y + 1 + dy] = rgb(a & 15)
    return im


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
        help_nes.ROWS, help_nes.TOP = 24, 0
        grid, _ = help_nes.page_cells(a.page, a.qr, quiet=True)
        cells, pals, sat, out = [b for row in grid[:24] for b in row], [0] * 768, [0xD0], a.dump
    else:
        data = open(a.dump, "rb").read()
        if len(data) != 1560:
            raise SystemExit("%s: %d bytes, a dump is 1560" % (a.dump, len(data)))
        if a.text:
            print("\n".join(text(data)))
            return
        cells, pals, sat, out = data[:768], data[768:1536], data[1536:], a.out
    im = draw(cells, pals, sat)
    im.resize((im.width * a.scale, im.height * a.scale), Image.NEAREST).save(out)


if __name__ == "__main__":
    main()

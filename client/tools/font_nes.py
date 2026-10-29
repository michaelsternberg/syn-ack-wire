#!/usr/bin/env python3
"""The NES client's 8x8 font (src/nes/font.txt) as a C header, and the tiles it makes.

Usage:
    font_nes.py font.txt > font.h

font.txt holds the 95 glyphs $20-$7E in order, each a line "0xNN name" and then
8 rows of 8 pixels, # lit and . dark; lines starting with ; are comments. After
them come the title logo's box glyphs, $01-$0B. The header has font_text[] (the
95 glyphs) and font_box[] (the box glyphs): a byte a row, bit 7 the leftmost
pixel. src/nes/text.c makes the background tiles from them at start-up.

tiles() gives the whole background pattern table as the client sets it, for the
tools that draw its screens (render_nes.py, help_nes.py), so they can't differ."""
import sys

FIRST, LAST = 0x20, 0x7E
BOX = 0x01                              # the box glyphs' first code
NOISE = 0x0C                            # the title's static: 4 tiles the NMI rewrites
QR = 0x10                               # quarter blocks: low nibble = TL, TR, BL, BR modules lit
INV = 0x80                              # a cell's inverse copy


def parse(path):
    """All the glyphs, as (code, name, rows): $20-$7E, then the box glyphs."""
    lines = [l.rstrip("\n") for l in open(path)]
    glyphs, i = [], 0
    while i < len(lines):
        line = lines[i]
        i += 1
        if not line.strip() or line.startswith(";"):
            continue
        head = line.split(None, 1)
        code = int(head[0], 16)
        n = len(glyphs)
        want = FIRST + n if n <= LAST - FIRST else BOX + n - (LAST - FIRST + 1)
        if code != want:
            sys.exit("%s:%d: glyph 0x%02X, expected 0x%02X" % (path, i, code, want))
        rows = lines[i:i + 8]
        if len(rows) < 8:
            sys.exit("%s:%d: glyph 0x%02X has %d rows, needs 8" % (path, i, code, len(rows)))
        for k, r in enumerate(rows):
            if len(r) != 8 or set(r) - set("#."):
                sys.exit("%s:%d: a row is 8 of # and ." % (path, i + k + 1))
        glyphs.append((code, head[1] if len(head) > 1 else "", rows))
        i += 8
    if len(glyphs) < LAST - FIRST + 1:
        sys.exit("%s: %d glyphs, needs %d and the box glyphs" % (path, len(glyphs), LAST - FIRST + 1))
    if len(glyphs) - (LAST - FIRST + 1) > NOISE - BOX:
        sys.exit("%s: over %d box glyphs" % (path, NOISE - BOX))
    return glyphs


def bits(rows):
    return [int(r.replace("#", "1").replace(".", "0"), 2) for r in rows]


def tiles(path):
    """The 256 background tiles as 8 rows of 8 colours (0-3), as text.c sets them: a glyph's
    pixels are colour 3 on colour 2 (plane 1 all set), its inverse copy the other way round;
    a quarter block's lit modules colour 1 on colour 0; the static, and anything else, 0."""
    pix = [[[0] * 8 for _ in range(8)] for _ in range(256)]
    for code, _, rows in parse(path):
        for y, b in enumerate(bits(rows)):
            for x in range(8):
                lit = b >> (7 - x) & 1
                pix[code][y][x] = 3 if lit else 2
                pix[code | INV][y][x] = 2 if lit else 3
    for n in range(16):
        for y in range(8):
            for x in range(8):
                bit = 8 >> ((y >= 4) * 2 + (x >= 4))
                pix[QR + n][y][x] = 1 if n & bit else 0
    return pix


def main():
    glyphs = parse(sys.argv[1])
    print("/* Made by tools/font_nes.py from src/nes/font.txt: edit that, not this. */")
    for label, part in (("font_text", [g for g in glyphs if g[0] >= FIRST]),
                        ("font_box", [g for g in glyphs if g[0] < FIRST])):
        print("static const unsigned char %s[%d] = {    /* $%02X-$%02X, 8 bytes each */"
              % (label, len(part) * 8, part[0][0], part[-1][0]))
        for code, name, rows in part:
            print("    %s,   /* $%02X %s */" % (", ".join("0x%02X" % b for b in bits(rows)), code,
                                              name.replace("*/", "* /")))
        print("};")
    print("#define FONT_BOXES %d" % len([g for g in glyphs if g[0] < FIRST]))


if __name__ == "__main__":
    main()

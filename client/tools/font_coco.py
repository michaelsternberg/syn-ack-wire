#!/usr/bin/env python3
"""The CoCo's 6x8 font (src/coco/font.txt) as a C header.

Usage:
    font_coco.py font.txt > font.h

font.txt holds the 95 glyphs $20-$7E in order, each a line "0xNN name" and then
8 rows of 6 pixels, # lit and . dark; lines starting with ; are comments. After
them come the title logo's box glyphs, from $60. The client's font is $20-$5F
and then the box glyphs: the server sends nothing above $5F, and the help pages,
the only lower case, are drawn from font.txt when the disk is built
(help_coco.py). The header has font[]: a byte a row, the 6 pixels in bits 7-2
(bit 7 the leftmost), each glyph's picture in a comment beside its bytes.

On stderr it lists the vertical strokes of 3 or more pixels on an odd column
(but in the box glyphs): on an NTSC picture they take the other artifact tint
from the rest."""
import sys

FIRST, LAST = 0x20, 0x7E
CLIENT_LAST = 0x5F                      # the client's font: up to here, then the box glyphs
BOX = 0x60                              # the box glyphs' first code


def read(path):
    """The 95 glyphs $20-$7E, as (code, name, rows)."""
    return parse(path)[:LAST - FIRST + 1]


def read_box(path):
    """The box glyphs, from $60."""
    return parse(path)[LAST - FIRST + 1:]


def parse(path):
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
        for n, r in enumerate(rows):
            if len(r) != 6 or set(r) - set("#."):
                sys.exit("%s:%d: a row is 6 of # and ." % (path, i + n + 1))
        glyphs.append((code, head[1] if len(head) > 1 else "", rows))
        i += 8
    if len(glyphs) < LAST - FIRST + 1:
        sys.exit("%s: %d glyphs, needs %d and the box glyphs" % (path, len(glyphs), LAST - FIRST + 1))
    if len(glyphs) - (LAST - FIRST + 1) > 0x80 - BOX:
        sys.exit("%s: over %d box glyphs" % (path, 0x80 - BOX))
    return glyphs


def odd_strokes(rows):
    """The odd columns with 3 or more lit pixels in a row, top to bottom."""
    found = []
    for x in range(1, 6, 2):
        run = 0
        for r in rows + ["......"]:
            if r[x] == "#":
                run += 1
            else:
                if run >= 3:
                    found.append(x)
                run = 0
    return sorted(set(found))


def main():
    box = read_box(sys.argv[1])
    glyphs = [g for g in read(sys.argv[1]) if g[0] <= CLIENT_LAST] + box
    print("/* Made by tools/font_coco.py from src/coco/font.txt: edit that, not this. */")
    print("static const unsigned char font[%d] = {" % (len(glyphs) * 8))
    for code, name, rows in glyphs:
        print("    /* 0x%02X %s */" % (code, name.replace("*/", "* /")))
        for r in rows:
            b = int(r.replace("#", "1").replace(".", "0"), 2) << 2
            print("    0x%02X,   /* %s */" % (b, r))
        odd = odd_strokes(rows)
        if odd and (code, name, rows) not in box:     # the hyphen's block is a block
            print("0x%02X %s: a stroke on column %s" % (code, name, ", ".join(map(str, odd))),
                  file=sys.stderr)
    print("};")


if __name__ == "__main__":
    main()

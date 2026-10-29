#!/usr/bin/env python3
"""The H89 instruction pages as one file of screen images, SYNACK.HLP.

Usage:
    help.py [--qr URL] OUT.HLP HELP01.TXT HELP02.TXT ...   check them, then write OUT

The pages are written as for MS-DOS (tools/help_msdos.py, which checks and lays them out: { and } for
reverse, \\ for the currency sign, [QR] for a QR code in half blocks), then each cell is turned into the
H19's: a character and its attributes, 1 reverse video (ESC p), 2 the graphics characters (ESC F). The
currency sign shows as Y, as the game shows it; the QR code's blocks are the graphics characters' top half
(▀), reversed for the bottom half and a reverse space for a whole block.

Each page becomes 1920 bytes: 24 rows of 40 cells, each a character then its attributes; page n is at
(n-1) * 1920 in OUT, which src/h89/display_text.c reads onto the screen."""
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
import help_msdos  # noqa: E402

INV, GFX = 1, 2
CELL = {help_msdos.YEN: (ord("Y"), 0), help_msdos.FULL: (ord(" "), INV),
        help_msdos.UPPER: (ord("p"), GFX), help_msdos.LOWER: (ord("p"), INV | GFX)}
ATTR = {help_msdos.NORMAL: 0, help_msdos.BRIGHT: 0, help_msdos.REVERSE: INV}


def h19(page):
    out = bytearray()
    for k in range(0, len(page), 2):
        ch, attr = page[k], page[k + 1]
        c, a = CELL.get(ch, (ch, 0))
        out += bytes((c, a ^ ATTR[attr]))
    return bytes(out)


def main():
    args = sys.argv[1:]
    url = None
    if args[:1] == ["--qr"]:
        url, args = args[1], args[2:]
    if len(args) < 2:
        sys.exit(__doc__)
    out, paths = args[0], args[1:]
    pages = [h19(p) for p in help_msdos.check(paths, url)]
    with open(out, "wb") as f:
        f.write(b"".join(pages))
    print("%s: %d help pages" % (out, len(pages)))


if __name__ == "__main__":
    main()

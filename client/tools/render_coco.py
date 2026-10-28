#!/usr/bin/env python3
"""Draw a CoCo PMODE 4 screen dump as the screen looks, for the manual's captures and mockups.

Usage:
    render_coco.py DUMP OUT.png [--scale N]

DUMP is the 6144 bytes of the PMODE 4 screen (256 x 192, one bit a pixel, bit 7 the leftmost; the client's is
at $0E00-$25FF), in colour set 0: a lit pixel is the VDG's green, a dark one its black, and the border is green.
Or it is 960 bytes, the client's 40 x 24 cells (txt_cells: ASCII, bit 7 inverse, as make pc-coco dumps them),
drawn as src/coco/text.c draws them (tools/help_coco.py). The colours are the ones MAME shows. No NTSC artifact
colour: MAME's snapshots, with artifacting on, show that."""
import argparse

from PIL import Image

import help_coco

GREEN = (48, 210, 0)
DARK = (38, 48, 22)
BX, BY = 16, 12             # the border around the 256 x 192 screen, in CoCo pixels


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("dump")
    ap.add_argument("out")
    ap.add_argument("--scale", type=int, default=3)
    a = ap.parse_args()
    data = open(a.dump, "rb").read()
    if len(data) == 960:
        scr = help_coco.Screen()
        cells = [[(b & 0x7F, bool(b & 0x80)) for b in data[r * 40:r * 40 + 40]] for r in range(24)]
        help_coco.draw_text(scr, cells, range(24), help_coco.glyphs())
        data = scr.bytes()
    if len(data) != 6144:
        raise SystemExit("%s: %d bytes, a PMODE 4 screen is 6144 (or 960 cells)" % (a.dump, len(data)))
    im = Image.new("RGB", (256 + 2 * BX, 192 + 2 * BY), GREEN)
    px = im.load()
    for y in range(192):
        for xb in range(32):
            b = data[y * 32 + xb]
            for bit in range(8):
                px[BX + xb * 8 + bit, BY + y] = GREEN if b & (0x80 >> bit) else DARK
    im.resize((im.width * a.scale, im.height * a.scale), Image.NEAREST).save(a.out)


if __name__ == "__main__":
    main()

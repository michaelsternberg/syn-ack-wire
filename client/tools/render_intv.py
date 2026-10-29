#!/usr/bin/env python3
"""Draw an Intellivision screen dump as a TV shows it, for the manual's captures and for checking.

Usage:
    render_intv.py DUMP OUT.png              a dump (tools/intvsim.py saves them)
    render_intv.py --text DUMP               the same as text

A dump is 993 bytes: BACKTAB's 240 words (20 x 12 cards, big-endian), GRAM's 64 cards (8 bytes each) and the
screen's mode (0 colour stack, with colored squares; 1 foreground/background). The GROM, Mattel's, isn't in this
repository: it is read from jzIntv's (JZINTV_DIR, default ~/Workspace/jzintv-20200712-src, rom/grom.bin). The
picture has an 8-pixel border round the 160 x 96 screen, each pixel 4 wide and each line doubled (as on a TV),
then doubled down: 704 x 416. A title's static is a still of it."""
import os
import sys

JZ = os.environ.get("JZINTV_DIR", os.path.expanduser("~/Workspace/jzintv-20200712-src"))

# the STIC's 16 colours, as jzIntv shows them
PAL = [(0x00, 0x00, 0x00), (0x00, 0x2D, 0xFF), (0xFF, 0x3D, 0x10), (0xC9, 0xCF, 0xAB),
       (0x38, 0x6B, 0x3F), (0x00, 0xA7, 0x56), (0xFA, 0xEA, 0x50), (0xFF, 0xFC, 0xFF),
       (0xBD, 0xAC, 0xC8), (0x24, 0xB8, 0xFF), (0xFF, 0xB4, 0x1F), (0x54, 0x6E, 0x00),
       (0xFF, 0x4E, 0x57), (0xA4, 0x96, 0xFF), (0x75, 0xCC, 0x80), (0xB5, 0x1A, 0x58)]
SIZE = 240 * 2 + 512 + 1


def pack(backtab, gram, mode):
    """A dump from BACKTAB's words, GRAM's bytes (512) and the mode."""
    return b"".join(w.to_bytes(2, "big") for w in backtab) + bytes(gram) + bytes([mode])


def unpack(d):
    if len(d) != SIZE:
        sys.exit("a dump is %d bytes, this is %d" % (SIZE, len(d)))
    return [int.from_bytes(d[2 * i:2 * i + 2], "big") for i in range(240)], d[480:992], d[992]


def text(d):
    """The 12 rows as text: colored squares as #, the yen sign as \\, other GRAM cards as %."""
    bt, _, mode = unpack(d)
    rows = []
    for r in range(12):
        s = ""
        for c in range(20):
            w = bt[r * 20 + c]
            if not mode and w & 0x1800 == 0x1000:
                s += "#" if w & 0x27FF else " "
            elif w & 0x800:
                s += "\\" if (w >> 3) & 0x3F == 0 else "%"
            else:
                card = (w >> 3) & (0x3F if mode else 0xFF)
                s += chr(card + 32) if card < 95 else "?"
        rows.append(s)
    return rows


def render(d, path):
    from PIL import Image
    bt, gram, mode = unpack(d)
    grom = open(os.path.join(JZ, "rom", "grom.bin"), "rb").read()
    img = Image.new("RGB", (176, 208), PAL[0])
    px = img.load()
    for r in range(12):
        for c in range(20):
            w = bt[r * 20 + c]
            if not mode and w & 0x1800 == 0x1000:      # colored squares: 4 squares of 4 x 4
                cols = [w & 7, (w >> 3) & 7, (w >> 6) & 7, (w >> 9) & 3 | (w >> 11) & 4]
                for y in range(8):
                    for x in range(8):
                        k = cols[(x >= 4) + 2 * (y >= 4)]
                        for s in (0, 1):
                            px[8 + c * 8 + x, 8 + (r * 8 + y) * 2 + s] = PAL[0 if k == 7 else k]
                continue
            g = bool(w & 0x800)
            if mode:
                n, fg = (w >> 3) & 0x3F, w & 7
                bg = (w >> 9 & 1) | (w >> 9 & 2) | ((w >> 13) & 1) << 2 | ((w >> 12) & 1) << 3
            else:
                n = (w >> 3) & (0x3F if g else 0xFF)
                fg, bg = (w & 7) | ((w >> 9) & 8 if g else 0), 0
            bits = gram[n * 8:n * 8 + 8] if g else grom[n * 8:n * 8 + 8]
            for y in range(8):
                for x in range(8):
                    col = PAL[fg] if (bits[y] >> (7 - x)) & 1 else PAL[bg]
                    for s in (0, 1):
                        px[8 + c * 8 + x, 8 + (r * 8 + y) * 2 + s] = col
    img.resize((176 * 4, 208 * 2), Image.NEAREST).save(path)


def main():
    if sys.argv[1:2] == ["--text"] and len(sys.argv) == 3:
        print("\n".join(text(open(sys.argv[2], "rb").read())))
    elif len(sys.argv) == 3:
        render(open(sys.argv[1], "rb").read(), sys.argv[2])
    else:
        sys.exit(__doc__)


if __name__ == "__main__":
    main()

#!/usr/bin/env python3
"""Write an Amiga .info icon (the WB 1.3-compatible DiskObject, 2 bitplanes) for the game's disk.

    make_icon.py tool    OUT.info     the game: a board of nodes
    make_icon.py script  OUT.info     the Start script: run by IconX

Index 1 and 2 are the two contrasting pens in every Workbench palette (1.3 and 2.x), 3 the accent, so the art reads on all."""
import struct
import sys

WBTOOL, WBPROJECT = 3, 4


def board(w=56, h=24):
    px = [[0] * w for _ in range(h)]

    def rect(x, y, a, b, c, fill=False):
        for j in range(y, y + b):
            for i in range(x, x + a):
                if fill or j in (y, y + b - 1) or i in (x, x + a - 1):
                    px[j][i] = c

    rect(0, 0, w, h, 1)
    rect(1, 1, w - 2, h - 2, 2, True)
    for r in range(3):                                  # three rows of nodes, joined by traces
        for c in range(5):
            x, y = 6 + c * 10, 4 + r * 7
            if c < 4:
                rect(x + 4, y + 2, 6, 1, 1, True)
            rect(x, y, 5, 5, 1)
            if (r * 5 + c) % 4 in (0, 3):
                rect(x + 1, y + 1, 3, 3, 3, True)
    return px


def play(w=40, h=18):
    px = [[0] * w for _ in range(h)]
    for j in range(h):
        for i in range(w):
            px[j][i] = 1 if j in (0, h - 1) or i in (0, w - 1) else 2
    for j in range(3, h - 3):                           # a triangle: run it
        for i in range(12, 12 + (min(j - 3, h - 4 - j) + 1) * 3):
            px[j][i] = 3
    return px


def planes(px):
    h, w = len(px), len(px[0])
    row = ((w + 15) // 16) * 2
    out = bytearray()
    for p in range(2):
        for line in px:
            b = bytearray(row)
            for x, v in enumerate(line):
                if v >> p & 1:
                    b[x >> 3] |= 0x80 >> (x & 7)
            out += b
    return bytes(out)


def info(kind, px, tool=None):
    h, w = len(px), len(px[0])
    d = struct.pack(">HH", 0xE310, 1)
    d += struct.pack(">IhhhhHHHIIIIIHI", 0, 0, 0, w, h, 0x0004, 0x0003, 0x0001, 1, 0, 0, 0, 0, 0, 0)
    d += struct.pack(">BBIIiiIII", kind, 0, 1 if tool else 0, 0, -0x80000000, -0x80000000, 0, 0, 16384)
    d += struct.pack(">hhhhhIBBI", 0, 0, w, h, 2, 1, 3, 0, 0) + planes(px)
    if tool:
        t = tool.encode() + b"\0"
        d += struct.pack(">I", len(t)) + t
    return d


if __name__ == "__main__":
    if len(sys.argv) != 3 or sys.argv[1] not in ("tool", "script"):
        sys.exit(__doc__)
    open(sys.argv[2], "wb").write(info(WBTOOL, board()) if sys.argv[1] == "tool" else info(WBPROJECT, play(), "C:IconX"))

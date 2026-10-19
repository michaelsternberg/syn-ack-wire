#!/usr/bin/env python3
"""Show an Atari 40x24 text screen dump (screen memory, internal codes) as text.

    tools/screen2txt.py scr012.bin

Inverse letters print in lowercase, other inverse characters as themselves, and
the yen glyph ($3C) as the yen sign. A frame is drawn around the 40 columns."""
import sys


def char(b):
    inv, c = b & 0x80, b & 0x7F
    if c == 0x3C:
        ch = "¥"
    elif c < 0x40:
        ch = chr(c + 0x20)
    elif c < 0x60:
        ch = "#"                        # ATASCII graphics characters
    else:
        ch = chr(c)
    return ch.lower() if inv and ch.isalpha() else ch


def show(data):
    print("+" + "-" * 40 + "+")
    for r in range(24):
        print("|" + "".join(char(b) for b in data[r * 40:r * 40 + 40]) + "|")
    print("+" + "-" * 40 + "+")


for f in sys.argv[1:]:
    show(open(f, "rb").read())

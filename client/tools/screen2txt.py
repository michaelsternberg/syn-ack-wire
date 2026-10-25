#!/usr/bin/env python3
"""Show an Atari 40x24 text screen dump (screen memory, internal codes) as text.

    tools/screen2txt.py scr012.bin
    tools/screen2txt.py --msdos SCR012.BIN

Inverse letters print in lowercase, other inverse characters as themselves, and
the yen glyph ($3C) as the yen sign. A frame is drawn around the 40 columns.

--msdos: a dump from the MS-DOS autoplay build (test/dos_dump.c), 40x24 cells
of character and attribute bytes. Code page 437; a reverse attribute (background
set) prints letters in lowercase, and a blinking cell shows as "*" in the margin."""
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


def dos_char(c, attr):
    ch = bytes([c]).decode("cp437") if c >= 0x20 else " "
    return ch.lower() if attr & 0x70 and ch.isalpha() else ch


def show_msdos(data):
    print("+" + "-" * 40 + "+")
    for r in range(24):
        cells = [(data[2 * i], data[2 * i + 1]) for i in range(r * 40, r * 40 + 40)]
        blink = "*" if any(a & 0x80 for _, a in cells) else ""
        print("|" + "".join(dos_char(c, a) for c, a in cells) + "|" + blink)
    print("+" + "-" * 40 + "+")


if __name__ == "__main__":
    args = sys.argv[1:]
    msdos = args[:1] == ["--msdos"]
    for f in args[1:] if msdos else args:
        (show_msdos if msdos else show)(open(f, "rb").read())

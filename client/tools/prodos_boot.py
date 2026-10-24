#!/usr/bin/env python3
"""Copy ProDOS's boot block onto a disk image AppleCommander made.

Usage:
    prodos_boot.py SOURCE TARGET.po

AppleCommander's -pro140 writes a boot block of its own, which only shows a
screen saying the disk was made with AppleCommander. This copies block 0, the
ProDOS loader that finds and runs the PRODOS file, from SOURCE, a 140K ProDOS
disk image: .po (ProDOS sector order) or .dsk/.do (DOS 3.3 order), into
TARGET.po."""
import sys

DOS_TO_PRODOS = [0x0, 0xE, 0xD, 0xC, 0xB, 0xA, 0x9, 0x8, 0x7, 0x6, 0x5, 0x4, 0x3, 0x2, 0x1, 0xF]


def block0(path):
    d = open(path, "rb").read()
    if len(d) != 143360:
        sys.exit("%s: %d bytes, not a 140K disk image" % (path, len(d)))
    if path.lower().endswith(".po"):
        b = d[:512]
    else:                               # DOS order: block 0 is sectors 0 and 14 of track 0
        b = d[DOS_TO_PRODOS[0] * 256:][:256] + d[DOS_TO_PRODOS[1] * 256:][:256]
    if b[0] != 0x01 or b"PRODOS" not in bytes(c & 0x7F for c in b):
        sys.exit("%s: block 0 isn't a ProDOS boot block" % path)
    return b


def main():
    src, dst = sys.argv[1], sys.argv[2]
    boot = block0(src)
    with open(dst, "r+b") as f:
        f.write(boot)


if __name__ == "__main__":
    main()

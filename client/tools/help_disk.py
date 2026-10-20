#!/usr/bin/env python3
"""Put the instruction pages on the game disk, as raw sectors the game reads with SIO.

    help_disk.py DISK.ATR FIRST_SECTOR help/atari/HELP01.TXT help/atari/HELP02.TXT ...
    help_disk.py --check help/atari/HELP01.TXT ...  (only check the pages)

Each page is one screen of ATASCII: at most 24 lines, each at most 40 characters,
ended by EOL ($9B). Pages must be numbered from 01 with no gaps. If the first line
ends in "n/m" (the heading), n must be the page's number and m the page count.

On the disk each page becomes 960 bytes of screen codes (24 rows of 40) in 8
sectors, page n starting at FIRST_SECTOR + 8*(n-1). The first sector holds the
last 64 screen bytes, then SIGNATURE; the other 7 hold the first 896. The game
reads the first sector, and only if the signature is there does it read the rest
onto the screen, so a missing page, or another disk in D1, leaves the screen
alone. The sectors are marked used in the DOS 2 VTOC so DOS won't write over
them; they have no directory entry.
"""
import re
import sys
from pathlib import Path

ROWS, COLS, EOL = 24, 40, 0x9B
SIGNATURE = b"SYNACK"
SECTOR = 128
VTOC = 360
LAST_SECTOR = 719       # the last one DOS 2's single-density VTOC covers


def screen_code(b):
    v = b & 0x7F
    if v < 0x20:
        v += 0x40
    elif v < 0x60:
        v -= 0x20
    return v | (b & 0x80)


def load(path, want, count):
    """The page as 960 screen bytes, and a list of errors."""
    data = path.read_bytes()
    if b"\n" in data and EOL not in data:
        return None, [f"{path.name}: has PC line endings, not ATASCII EOL ($9B)"]
    lines = data.split(bytes([EOL]))
    if lines and lines[-1] == b"":
        lines.pop()                         # the last line's EOL
    errs = []
    m = re.fullmatch(r"HELP(\d\d)\.TXT", path.name)
    if not m or int(m.group(1)) != want:
        errs.append(f"{path.name}: expected HELP{want:02d}.TXT (pages are numbered 01, 02... with no gaps)")
    if len(lines) > ROWS:
        errs.append(f"{path.name}: {len(lines)} lines, max {ROWS}")
    for n, line in enumerate(lines, 1):
        if len(line) > COLS:
            errs.append(f"{path.name} line {n}: {len(line)} characters, max {COLS}")
    if lines:
        head = bytes(c & 0x7F for c in lines[0]).decode("latin-1")
        m = re.search(r"(\d+)/(\d+)\s*$", head)
        if m and (int(m.group(1)), int(m.group(2))) != (want, count):
            errs.append(f"{path.name}: heading says {m.group(0).strip()}, expected {want}/{count}")
    scr = bytearray(ROWS * COLS)
    for r, line in enumerate(lines[:ROWS]):
        for c, b in enumerate(line[:COLS]):
            scr[r * COLS + c] = screen_code(b)
    return bytes(scr), errs


def main():
    args = sys.argv[1:]
    check = args[:1] == ["--check"]
    if check:
        args = args[1:]
    else:
        if len(args) < 2:
            sys.exit(__doc__)
        atr, first, args = Path(args[0]), int(args[1]), args[2:]
    paths = sorted(Path(p) for p in args)
    if not paths:
        sys.exit("no help pages")
    pages, errs = [], []
    for want, path in enumerate(paths, 1):
        scr, e = load(path, want, len(paths))
        pages.append(scr)
        errs += e
    if errs:
        sys.exit("\n".join(errs))
    if check:
        print(f"help pages OK: {len(paths)}")
        return

    disk = bytearray(atr.read_bytes())
    if disk[:2] != b"\x96\x02" or disk[4] | disk[5] << 8 != SECTOR:
        sys.exit(f"{atr}: not a single- or enhanced-density ATR")

    def at(n):
        return 16 + (n - 1) * SECTOR

    vtoc = at(VTOC)
    last = first + 8 * len(pages) - 1
    if first < 4 or last > LAST_SECTOR:
        sys.exit(f"sectors {first}-{last} don't fit between sectors 4 and {LAST_SECTOR}")
    for n in range(first, last + 1):
        byte, bit = vtoc + 10 + n // 8, 0x80 >> (n % 8)
        if not disk[byte] & bit:
            sys.exit(f"{atr}: sector {n} is already used by DOS; move the help pages (HELP_SECTOR)")
        disk[byte] &= ~bit
    free = disk[vtoc + 3] | disk[vtoc + 4] << 8
    free -= last + 1 - first
    disk[vtoc + 3], disk[vtoc + 4] = free & 0xFF, free >> 8

    for i, scr in enumerate(pages):
        s = first + 8 * i
        head = scr[7 * SECTOR:] + SIGNATURE
        disk[at(s):at(s + 1)] = head.ljust(SECTOR, b"\0")
        disk[at(s + 1):at(s + 8)] = scr[:7 * SECTOR]
    atr.write_bytes(disk)
    print(f"{atr}: {len(pages)} help pages in sectors {first}-{last}")


if __name__ == "__main__":
    main()

#!/usr/bin/env python3
"""Put the instruction pages on the game disk, as raw sectors the game reads with SIO.

    help_disk.py [--qr URL] DISK.ATR FIRST_SECTOR help/atari/screen/HELP01.TXT help/atari/screen/HELP02.TXT ...
    help_disk.py [--qr URL] --check help/atari/screen/HELP01.TXT ...  (only check the pages)

Each page is one screen of ATASCII: at most 24 lines, each at most 40 characters,
ended by EOL ($9B). Pages must be numbered from 01 with no gaps. If the first line
ends in "n/m" (the heading), n must be the page's number and m the page count.

"[QR]" on a page is replaced by a QR code for --qr's URL, its top left corner where the
marker is. Each character holds 2x2 modules: dark modules are the dark background, light
ones lit pixels (quarter and half blocks), with a light quiet zone of 2-3 modules around.
The code gets the highest error correction level that fits in blanks from the marker, and
of the eight masks and four ways of laying the 2x2 grid over it, the one that leaves the
fewest cells with two diagonal light modules: the font has no glyph for those, so they
are drawn with one, a module wrong each (the count is printed).

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

import qr

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


# The ATASCII glyph that lights quadrants (top left, top right, bottom left, bottom right),
# read from the OS ROM font; the two diagonals have none, so they lose their right-hand module.
QUADS = {
    (0, 0, 0, 0): 0x20, (0, 0, 0, 1): 0x09, (0, 0, 1, 0): 0x0F, (0, 0, 1, 1): 0x15,
    (0, 1, 0, 0): 0x0B, (0, 1, 0, 1): 0x99, (0, 1, 1, 1): 0x8C, (1, 0, 0, 0): 0x0C,
    (1, 0, 1, 0): 0x19, (1, 0, 1, 1): 0x8B, (1, 1, 0, 0): 0x95, (1, 1, 0, 1): 0x8F,
    (1, 1, 1, 0): 0x89, (1, 1, 1, 1): 0xA0,
    (0, 1, 1, 0): 0x0F, (1, 0, 0, 1): 0x0C,
}
DIAGONALS = ((0, 1, 1, 0), (1, 0, 0, 1))
QR_MARK = b"[QR]"


def qr_cells(url, level, version):
    """The QR code as rows of ATASCII, and how many modules the diagonals got wrong."""
    best = None
    size = 17 + 4 * version
    n = (size + 5) // 2                             # characters a side: 2-3 modules of quiet zone
    for mask in range(8):
        dark = qr.encode(url, level, version, mask)
        for ox in (2, 3):
            for oy in (2, 3):
                def lit(r, c):
                    r, c = r - oy, c - ox
                    return int(not (0 <= r < size and 0 <= c < size and dark[r][c]))
                cells = [[(lit(2 * y, 2 * x), lit(2 * y, 2 * x + 1), lit(2 * y + 1, 2 * x), lit(2 * y + 1, 2 * x + 1))
                          for x in range(n)] for y in range(n)]
                wrong = sum(q in DIAGONALS for row in cells for q in row)
                key = (wrong, qr.penalty(dark))
                if best is None or key < best[0]:
                    best = key, cells
    (wrong, _), cells = best
    return [bytes(QUADS[q] for q in row) for row in cells], wrong


def place_qr(lines, url, name):
    """Draw the QR code over its marker in `lines` (bytearrays, 24 of 40). Errors, and a note."""
    marks = [(r, line.find(QR_MARK)) for r, line in enumerate(lines) if QR_MARK in line]
    if not marks:
        return [], None
    if len(marks) > 1 or lines[marks[0][0]].count(QR_MARK) > 1:
        return [f"{name}: more than one {QR_MARK.decode()}"], None
    if not url:
        return [f"{name}: has {QR_MARK.decode()} but no --qr URL"], None
    r0, c0 = marks[0]
    lines[r0][c0:c0 + len(QR_MARK)] = b" " * len(QR_MARK)

    def blank(n):
        return r0 + n <= ROWS and c0 + n <= COLS and all(
            lines[r][c0:c0 + n] == b" " * n for r in range(r0, r0 + n))
    for level in "HQML":
        version = next((v for v in qr.BLOCKS if qr.fits(url, v, level)), None)
        if version and blank((17 + 4 * version + 5) // 2):
            break
    else:
        return [f"{name}: no room for a QR code of {url!r} at row {r0 + 1}, column {c0 + 1} "
                f"(it needs blanks down and to the right)"], None
    rows, wrong = qr_cells(url, level, version)
    for i, row in enumerate(rows):
        lines[r0 + i][c0:c0 + len(row)] = row
    return [], (f"{name}: QR code version {version}-{level}, {len(rows)}x{len(rows)} characters, "
                f"{wrong} module{'s' * (wrong != 1)} approximated")


def load(path, want, count, url=None):
    """The page as 960 screen bytes, a list of errors, and a note (or None)."""
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
    page = [bytearray(line[:COLS].ljust(COLS, b" ")) for line in lines[:ROWS]]
    page += [bytearray(b" " * COLS) for _ in range(ROWS - len(page))]
    e, note = place_qr(page, url, path.name)
    errs += e
    scr = bytearray(ROWS * COLS)
    for r, line in enumerate(page):
        for c, b in enumerate(line):
            scr[r * COLS + c] = screen_code(b)
    return bytes(scr), errs, note


def main():
    args = sys.argv[1:]
    url = None
    if args[:1] == ["--qr"]:
        if len(args) < 2:
            sys.exit(__doc__)
        url, args = args[1], args[2:]
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
    pages, errs, notes = [], [], []
    for want, path in enumerate(paths, 1):
        scr, e, note = load(path, want, len(paths), url)
        pages.append(scr)
        errs += e
        notes += [note] if note else []
    if errs:
        sys.exit("\n".join(errs))
    if check:
        print("\n".join(notes + [f"help pages OK: {len(paths)}"]))
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

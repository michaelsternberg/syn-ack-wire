#!/usr/bin/env python3
"""The NES instruction pages, packed into a C header the game is built with (they go in its ROM);
the Adam's too (--rows 24 --top 0: its screen has no overscan to keep out of), and the
Intellivision's (--rows 12 --cols 20 --top 0 --left 0 --bas: 20 x 12 cards, as IntyBASIC DATA).

Usage:
    help_nes.py [--rows N --cols N --top R --left C] [--bas] [--qr URL] OUT HELP01.TXT ...  check them, write OUT
    help_nes.py [--rows N --cols N --top R --left C] [--qr URL] --check HELP01.TXT ...      only check

A page is plain ASCII: at most 28 lines of at most 30 characters, the NES's
screen inside a TV's overscan (src/nes/text.h: name table rows 1-28, columns
1-30). Text between { and } shows in inverse, and "\\" shows as the yen sign,
as the server's text does. Pages are numbered 01, 02... with no gaps, and a
heading ending in n/m must have n = the page number and m = the page count.

A page with [QR] on it shows a QR code for URL from the marker's line down,
centred across the screen: dark modules on white, 2x2 modules a tile, in the
quarter-block tiles $10-$1F (src/nes/text.c), so every module is exact, with
a 4-module white border, at the highest error correction level that fits in
20 lines. Only the marker may be on the lines the code covers.

The header has help_data[], every page's 28 lines one after another, each
ended by 0: a byte is a tile (a character, | $80 inverse, or a quarter
block), or 1-15 for that many spaces; and help_page[], where each page starts.
src/nes/help.c (src/adam/help.c) draws a page from them, its first line on
screen row HELP_TOP, from column HELP_LEFT. page_cells() gives a page as the 30 x 32
cells help.c draws, for render_nes.py."""
import os
import re
import sys

import qr

ROWS, COLS = 28, 30
TOP, LEFT = 1, 1                # the first line's screen row and column (--top)
QUIET = 4                       # the white border, in modules
QR_LINES = 20
QR_TILE = 0x10                  # quarter blocks: low nibble = top left, top right, bottom left, bottom right lit
INV = 0x80


def qr_code(url):
    """The code for `url`: the highest level, then the smallest version, that fits QR_LINES lines."""
    for level in "HQML":
        for version in range(1, 11):
            if qr.fits(url, version, level) and (17 + 4 * version + 2 * QUIET + 1) // 2 <= QR_LINES:
                return qr.encode(url, level, version), version, level
    sys.exit("%s: too long for a QR code on a help page" % url)


def page_cells(path, url=None, quiet=False):
    """The page as 30 rows of 32 cells (name table rows 1-28 at index 1-28: TOP to TOP + ROWS - 1), and its lines."""
    lines = open(path).read().rstrip("\n").split("\n")
    if len(lines) > ROWS:
        sys.exit("%s: %d lines, at most %d" % (path, len(lines), ROWS))
    cells = [[0x20] * 32 for _ in range(30)]
    qr_at = [r for r, line in enumerate(lines) if line.strip() == "[QR]"]
    covered = set()
    if qr_at:
        if not url:
            sys.exit("%s: has [QR] but no --qr URL" % path)
        m, version, level = qr_code(url)
        n = len(m)
        size = (n + 2 * QUIET + 1) // 2         # tiles
        r0, c0 = qr_at[0], 1 + (COLS - size) // 2
        if r0 + size > ROWS:
            sys.exit("%s:%d: the QR code runs off the page" % (path, r0 + 1))
        covered = set(range(r0, r0 + size))

        def light(y, x):
            y, x = y - QUIET, x - QUIET
            return not (0 <= x < n and 0 <= y < n and m[y][x])

        for ty in range(size):
            for tx in range(size):
                bits = 0
                for k, (dy, dx) in enumerate(((0, 0), (0, 1), (1, 0), (1, 1))):
                    if light(2 * ty + dy, 2 * tx + dx):
                        bits |= 8 >> k
                cells[TOP + r0 + ty][c0 + tx] = QR_TILE | bits
        if not quiet:
            print("%s: QR code version %d-%s, %dx%d modules, %d tiles square with its border"
                  % (os.path.basename(path), version, level, n, n, size))
    for r, line in enumerate(lines):
        if r in covered:
            if line.strip() not in ("", "[QR]"):
                sys.exit("%s:%d: only the [QR] marker goes on the lines the code covers" % (path, r + 1))
            continue
        inv, c = False, 0
        for ch in line:
            if ch in "{}":
                inv = ch == "{"
                continue
            if not " " <= ch <= "~":
                sys.exit("%s:%d: %r is not printable ASCII" % (path, r + 1, ch))
            if c >= COLS:
                sys.exit("%s:%d: over %d characters" % (path, r + 1, COLS))
            cells[TOP + r][LEFT + c] = ord(ch) | (INV if inv else 0)
            c += 1
    return cells, lines


def pack(cells):
    """ROWS lines, each its tiles from column LEFT with runs of spaces counted, ended by 0."""
    out = bytearray()
    for r in range(TOP, TOP + ROWS):
        row = cells[r][LEFT:LEFT + COLS]
        while row and row[-1] == 0x20:
            row = row[:-1]
        c = 0
        while c < len(row):
            if row[c] == 0x20:
                k = c
                while k < len(row) and row[k] == 0x20 and k - c < 15:
                    k += 1
                out.append(k - c)
                c = k
            else:
                out.append(row[c])
                c += 1
        out.append(0)
    return bytes(out)


def check(paths, url):
    for n, path in enumerate(paths, 1):
        m = re.match(r"HELP(\d\d)\.TXT$", os.path.basename(path))
        if not m or int(m.group(1)) != n:
            sys.exit("%s: expected HELP%02d.TXT" % (path, n))
    pages = []
    for n, path in enumerate(paths, 1):
        cells, lines = page_cells(path, url)
        for line in lines:
            m = re.search(r"(\d+)/(\d+)\s*}?\s*$", line)
            if m and (int(m.group(1)), int(m.group(2))) != (n, len(paths)):
                sys.exit("%s: says %s/%s, is %d/%d" % (path, m.group(1), m.group(2), n, len(paths)))
        pages.append(pack(cells))
    return pages


def header(pages, folder):
    out = ["/* Made by tools/help_nes.py from %s/HELP*.TXT: edit those, not this. */" % folder,
           "#define HELP_PAGES %d" % len(pages),
           "#define HELP_ROWS %d" % ROWS,
           "#define HELP_TOP %d" % TOP,
           "#define HELP_LEFT %d" % LEFT]
    at, starts = 0, []
    for p in pages:
        starts.append(at)
        at += len(p)
    out.append("static const unsigned help_page[HELP_PAGES] = { %s };" % ", ".join(map(str, starts)))
    out.append("static const unsigned char help_data[%d] = {" % at)
    data = b"".join(pages)
    for i in range(0, len(data), 16):
        out.append("    %s," % ", ".join("0x%02X" % b for b in data[i:i + 16]))
    out.append("};")
    return "\n".join(out) + "\n"


def bas(pages, folder):
    """IntyBASIC: help_page() each page's first byte, help_data the bytes two to a word (DATA PACKED, the
    first in the high byte), as src/intv/help.bas reads them."""
    out = ["' Made by tools/help_nes.py from %s/HELP*.TXT: edit those, not this." % folder,
           "    CONST HELP_PAGES = %d" % len(pages)]
    at, starts = 0, []
    for p in pages:
        starts.append(at)
        at += len(p)
    out.append("help_page:")
    out.append("    DATA %s" % ", ".join(map(str, starts)))
    out.append("help_data:")
    data = b"".join(pages)
    for i in range(0, len(data), 16):
        out.append("    DATA PACKED %s" % ", ".join("$%02X" % b for b in data[i:i + 16]))
    return "\n".join(out) + "\n"


def main():
    global ROWS, COLS, TOP, LEFT
    args = sys.argv[1:]
    url = None
    out_bas = False
    while args[:1] in (["--rows"], ["--cols"], ["--top"], ["--left"], ["--bas"]):
        if args[0] == "--bas":
            out_bas, args = True, args[1:]
            continue
        n = int(args[1])
        if args[0] == "--rows":
            ROWS = n
        elif args[0] == "--cols":
            COLS = n
        elif args[0] == "--top":
            TOP = n
        else:
            LEFT = n
        args = args[2:]
    if args[:1] == ["--qr"]:
        url, args = args[1], args[2:]
    if args and args[0] == "--check":
        check(args[1:], url)
        return
    out, paths = args[0], args[1:]
    pages = check(paths, url)
    with open(out, "w") as f:
        f.write((bas if out_bas else header)(pages, os.path.dirname(os.path.relpath(paths[0])) if paths else "."))
    print("%s: %d help pages, %d bytes" % (out, len(pages), sum(map(len, pages))))


if __name__ == "__main__":
    main()

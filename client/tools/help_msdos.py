#!/usr/bin/env python3
"""The MS-DOS instruction pages as one file of screen images, SYNACK.HLP.

Usage:
    help_msdos.py [--qr URL] OUT.HLP HELP01.TXT HELP02.TXT ...   check them, then write OUT
    help_msdos.py [--qr URL] --check HELP01.TXT ...                only check

A page is plain ASCII, as for the Apple II (help_apple2.py): at most 24 lines of
at most 40 characters, in normal video. Text between { and } shows in reverse
(any character: the PC has reverse lower case), and "\\" shows as code page
437's yen sign, as the server's text does. Pages are numbered 01, 02... with no
gaps, and a heading ending in n/m must have n = the page number and m = the
page count.

A page with [QR] on it shows a QR code for URL on lines 1-20, drawn with the
half-block characters, two modules to a character cell (white on black, with
a 4-module white border), at the highest error correction level that fits
(tools/qr.py); lines 21-24 stay text. Only the marker may be on lines 1-20.

Each page becomes 1920 bytes: 24 rows of 40 cells, each a character then its
attribute, as in DOS video memory; page n is at (n-1) * 1920 in OUT, which
src/msdos/display_text.c reads onto the screen."""
import os
import re
import sys

import qr

NORMAL, REVERSE, BRIGHT = 0x07, 0x70, 0x0F
YEN = 0x9D
FULL, UPPER, LOWER = 0xDB, 0xDF, 0xDC          # code page 437 blocks: whole, top half, bottom half
QUIET = 4                                       # the white border, in modules


def text_row(cells, path, r, line):
    attr, c = NORMAL, 0
    for ch in line:
        if ch == "{" or ch == "}":
            attr = REVERSE if ch == "{" else NORMAL
            continue
        if not " " <= ch <= "~":
            sys.exit("%s:%d: %r is not printable ASCII" % (path, r + 1, ch))
        if c >= 40:
            sys.exit("%s:%d: over 40 characters" % (path, r + 1))
        cells[r * 40 + c] = (YEN if ch == "\\" else ord(ch), attr)
        c += 1


def qr_code(url):
    """The code for `url`: the highest level, then the smallest version, that fits 40 x 40 modules with its border."""
    for level in "HQML":
        for version in range(1, 7):
            if qr.fits(url, version, level) and 17 + 4 * version + 2 * QUIET <= 40:
                return qr.encode(url, level, version), version, level
    sys.exit("%s: too long for a QR code on a help page" % url)


def qr_page(cells, path, lines, url):
    if not url:
        sys.exit("%s: has [QR] but no --qr URL" % path)
    for r, line in enumerate(lines[:20]):
        if line.strip() not in ("", "[QR]"):
            sys.exit("%s:%d: a [QR] page has the code on lines 1-20: only the marker goes there" % (path, r + 1))
    m, version, level = qr_code(url)
    n = len(m)
    size = n + 2 * QUIET
    x0 = (40 - size) // 2
    y0 = (40 - size) // 2
    light = [[False] * 40 for _ in range(40)]
    for by in range(size):
        for bx in range(size):
            y, x = by - QUIET, bx - QUIET
            light[y0 + by][x0 + bx] = not (0 <= x < n and 0 <= y < n and m[y][x])
    for r in range(20):                         # a character cell is two modules: top and bottom
        for c in range(40):
            top, bottom = light[2 * r][c], light[2 * r + 1][c]
            ch = FULL if top and bottom else UPPER if top else LOWER if bottom else ord(" ")
            cells[r * 40 + c] = (ch, BRIGHT)
    print("%s: QR code version %d-%s, %dx%d modules, %d with its border"
          % (os.path.basename(path), version, level, n, n, size))


def page_image(path, url):
    lines = open(path).read().rstrip("\n").split("\n")
    if len(lines) > 24:
        sys.exit("%s: %d lines, at most 24" % (path, len(lines)))
    cells = [(ord(" "), NORMAL)] * (24 * 40)
    code = any(line.strip() == "[QR]" for line in lines)
    for r, line in enumerate(lines):
        if not (code and r < 20):
            text_row(cells, path, r, line)
    if code:
        qr_page(cells, path, lines, url)
    return bytes(b for ch, attr in cells for b in (ch, attr)), lines


def check(paths, url):
    for n, path in enumerate(paths, 1):
        m = re.match(r"HELP(\d\d)\.TXT$", os.path.basename(path))
        if not m or int(m.group(1)) != n:
            sys.exit("%s: expected HELP%02d.TXT" % (path, n))
    pages = []
    for n, path in enumerate(paths, 1):
        img, lines = page_image(path, url)
        for line in lines[:2] + lines[20:22]:   # the heading: on top, or under a QR code
            m = re.search(r"(\d+)/(\d+)\s*}?\s*$", line)
            if m and (int(m.group(1)), int(m.group(2))) != (n, len(paths)):
                sys.exit("%s: says %s/%s, is %d/%d" % (path, m.group(1), m.group(2), n, len(paths)))
        pages.append(img)
    return pages


def main():
    args = sys.argv[1:]
    url = None
    if args[:1] == ["--qr"]:
        url, args = args[1], args[2:]
    if args and args[0] == "--check":
        check(args[1:], url)
        return
    out, paths = args[0], args[1:]
    pages = check(paths, url)
    with open(out, "wb") as f:
        f.write(b"".join(pages))
    print("%s: %d help pages" % (out, len(pages)))


if __name__ == "__main__":
    main()

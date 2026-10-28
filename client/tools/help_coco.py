#!/usr/bin/env python3
"""The CoCo 1/2 instruction pages as one file of PMODE 4 screen images, SYNACK.HLP.

Usage:
    help_coco.py [--qr URL] OUT.HLP HELP01.TXT HELP02.TXT ...   check them, then write OUT
    help_coco.py [--qr URL] --check HELP01.TXT ...                only check

A page is plain ASCII, as for MS-DOS (help_msdos.py): at most 24 lines of at
most 40 characters. Text between { and } shows in inverse, and "\\" shows as
the yen sign, as the server's text does. Pages are numbered 01, 02... with no
gaps, and a heading ending in n/m must have n = the page number and m = the
page count.

A page with [QR] on it shows a QR code for URL on lines 1-20, in pixels (dark
modules on green, with a 4-module green border), at the highest error
correction level that fits and the largest whole number of pixels a module;
lines 21-24 stay text. Only the marker may be on lines 1-20.

Each page becomes 6144 bytes, the PMODE 4 screen as src/coco/text.c draws it
(the font from src/coco/font.txt, the inverse margins too), green on black;
page n is at (n-1) * 6144 in OUT, which src/coco/help.c reads onto the screen."""
import os
import re
import sys

import font_coco
import qr

QUIET = 4                       # the green border, in modules
SHORT_RUN = 4                   # text.c: inverse runs shorter than this get the line above
FONT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "src", "coco", "font.txt")


class Screen:
    """256 x 192 pixels, 1 = green."""

    def __init__(self):
        self.px = [[0] * 256 for _ in range(192)]

    def set(self, x, y, v):
        self.px[y][x] = v

    def bytes(self):
        out = bytearray()
        for row in self.px:
            for xb in range(32):
                b = 0
                for bit in range(8):
                    b = b << 1 | row[xb * 8 + bit]
                out.append(b)
        return bytes(out)


def glyphs():
    return {code: rows for code, _, rows in font_coco.read(FONT)}


def draw_text(scr, cells, rows_drawn, font):
    """cells: 24 x 40 (code, inverse). As text.c: each cell 6 x 8 from pixel 8 + 6c, then the margins
    inverse runs take: the column left of each run, and the line above each short run."""
    for r in rows_drawn:
        for c in range(40):
            code, inv = cells[r][c]
            for y, line in enumerate(font[code]):
                for x, p in enumerate(line):
                    scr.set(8 + 6 * c + x, 8 * r + y, (p == "#") ^ inv)
    for r in rows_drawn:
        c = 0
        while c < 40:
            if not cells[r][c][1]:
                c += 1
                continue
            start = c
            while c < 40 and cells[r][c][1]:
                c += 1
            for y in range(8):
                scr.set(7 + 6 * start, 8 * r + y, 1)
            if r and c - start < SHORT_RUN:
                for x in range(7 + 6 * start, 7 + 6 * c + 1):
                    scr.set(x, 8 * r - 1, 1)


def text_row(cells, path, r, line):
    inv, c = False, 0
    for ch in line:
        if ch == "{" or ch == "}":
            inv = ch == "{"
            continue
        if not " " <= ch <= "~":
            sys.exit("%s:%d: %r is not printable ASCII" % (path, r + 1, ch))
        if c >= 40:
            sys.exit("%s:%d: over 40 characters" % (path, r + 1))
        cells[r][c] = (ord(ch), inv)
        c += 1


def qr_code(url):
    """The code for `url`: the highest level, then the smallest version, that fits 20 text rows (160 pixels)."""
    for level in "HQML":
        for version in range(1, 11):
            if qr.fits(url, version, level) and 17 + 4 * version + 2 * QUIET <= 160:
                return qr.encode(url, level, version), version, level
    sys.exit("%s: too long for a QR code on a help page" % url)


def qr_page(scr, path, lines, url):
    if not url:
        sys.exit("%s: has [QR] but no --qr URL" % path)
    for r, line in enumerate(lines[:20]):
        if line.strip() not in ("", "[QR]"):
            sys.exit("%s:%d: a [QR] page has the code on lines 1-20: only the marker goes there" % (path, r + 1))
    m, version, level = qr_code(url)
    n = len(m)
    size = n + 2 * QUIET
    scale = 160 // size
    x0 = (256 - size * scale) // 2
    y0 = (160 - size * scale) // 2
    for by in range(size):
        for bx in range(size):
            y, x = by - QUIET, bx - QUIET
            light = not (0 <= x < n and 0 <= y < n and m[y][x])
            for dy in range(scale):
                for dx in range(scale):
                    scr.set(x0 + bx * scale + dx, y0 + by * scale + dy, int(light))
    print("%s: QR code version %d-%s, %dx%d modules, %d with its border, %d pixels a module"
          % (os.path.basename(path), version, level, n, n, size, scale))


def page_image(path, url, font):
    lines = open(path).read().rstrip("\n").split("\n")
    if len(lines) > 24:
        sys.exit("%s: %d lines, at most 24" % (path, len(lines)))
    cells = [[(ord(" "), False)] * 40 for _ in range(24)]
    code = any(line.strip() == "[QR]" for line in lines)
    for r, line in enumerate(lines):
        if not (code and r < 20):
            text_row(cells, path, r, line)
    scr = Screen()
    draw_text(scr, cells, range(20, 24) if code else range(24), font)
    if code:
        qr_page(scr, path, lines, url)
    return scr.bytes(), lines


def check(paths, url):
    font = glyphs()
    for n, path in enumerate(paths, 1):
        m = re.match(r"HELP(\d\d)\.TXT$", os.path.basename(path))
        if not m or int(m.group(1)) != n:
            sys.exit("%s: expected HELP%02d.TXT" % (path, n))
    pages = []
    for n, path in enumerate(paths, 1):
        img, lines = page_image(path, url, font)
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

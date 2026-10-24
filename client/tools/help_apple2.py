#!/usr/bin/env python3
"""The Apple II instruction pages (apple2_text) as one file of text-page images.

Usage:
    help_apple2.py [--qr URL] OUT.HLP HELP01.TXT HELP02.TXT ...   check them, then write OUT
    help_apple2.py [--qr URL] --check HELP01.TXT ...                only check

A page is plain ASCII: at most 24 lines of at most 40 characters, shown as
typed in normal video. Text between { and } shows in inverse (upper case,
digits and punctuation only: the character set has no inverse lower case).
Pages are numbered 01, 02... with no gaps, and a heading ending in n/m must
have n = the page number and m = the page count, as on the Atari
(help_disk.py).

A page with [QR] on it is a lo-res page: lines 1-20 are 40 x 40 blocks of
graphics, where a QR code for URL is drawn in the middle, black on white
with a 4-module white border, at the highest error correction level that
fits (tools/qr.py); lines 21-24 stay text, as in the Apple's mixed mode. Only
the marker may be on lines 1-20.

Each page becomes 1024 bytes, an image of text page 2 ($0800) with its rows in
the screen's interleaved order; page n is at (n-1) * 1024 in OUT, which
src/apple2/help.s reads straight onto the screen. A lo-res page has LORES in
its first screen hole (offset $78, never shown): help.s shows it in lo-res."""
import os
import re
import sys

import qr

NORMAL, INV = 0x80, 0x00
LORES, HOLE = 0x4C, 0x78        # help.s: the flag, and where it is
WHITE = 15                      # lo-res colours; black is 0
QUIET = 4                       # the white border, in modules


def row_offset(r):
    return (r & 7) * 0x80 + (r >> 3) * 40


def text_row(img, path, r, line):
    mode, c = NORMAL, 0
    for ch in line:
        if ch == "{" or ch == "}":
            mode = INV if ch == "{" else NORMAL
            continue
        if not " " <= ch <= "~":
            sys.exit("%s:%d: %r is not printable ASCII" % (path, r + 1, ch))
        if mode == INV and ch >= "`":
            sys.exit("%s:%d: no inverse lower case (%r)" % (path, r + 1, ch))
        if c >= 40:
            sys.exit("%s:%d: over 40 characters" % (path, r + 1))
        img[row_offset(r) + c] = ord(ch) | 0x80 if mode == NORMAL else ord(ch) & 0x3F
        c += 1


def qr_code(url):
    """The code for `url`: the highest level, then the smallest version, that fits 40 blocks with its border."""
    for level in "HQML":
        for version in range(1, 7):
            if qr.fits(url, version, level) and 17 + 4 * version + 2 * QUIET <= 40:
                return qr.encode(url, level, version), version, level
    sys.exit("%s: too long for a QR code on a lo-res page" % url)


def lores_page(img, path, lines, url):
    if not url:
        sys.exit("%s: has [QR] but no --qr URL" % path)
    for r, line in enumerate(lines[:20]):
        if line.strip() not in ("", "[QR]"):
            sys.exit("%s:%d: a [QR] page has graphics on lines 1-20: only the marker goes there" % (path, r + 1))
    m, version, level = qr_code(url)
    n = len(m)
    size = n + 2 * QUIET
    x0 = y0 = (40 - size) // 2
    blocks = [[0] * 40 for _ in range(40)]
    for by in range(size):
        for bx in range(size):
            y, x = by - QUIET, bx - QUIET
            if not (0 <= x < n and 0 <= y < n and m[y][x]):
                blocks[y0 + by][x0 + bx] = WHITE
    for r in range(20):                         # a text row's byte is two blocks: low nibble on top
        for c in range(40):
            img[row_offset(r) + c] = blocks[2 * r][c] | blocks[2 * r + 1][c] << 4
    img[HOLE] = LORES
    print("%s: QR code version %d-%s, %dx%d modules, %d blocks with its border"
          % (os.path.basename(path), version, level, n, n, size))


def page_image(path, url):
    lines = open(path).read().rstrip("\n").split("\n")
    if len(lines) > 24:
        sys.exit("%s: %d lines, at most 24" % (path, len(lines)))
    img = bytearray(1024)
    for r in range(24):
        img[row_offset(r):row_offset(r) + 40] = bytes([ord(" ") | NORMAL]) * 40
    lores = any(line.strip() == "[QR]" for line in lines)
    for r, line in enumerate(lines):
        if not (lores and r < 20):
            text_row(img, path, r, line)
    if lores:
        lores_page(img, path, lines, url)
    return bytes(img), lines


def check(paths, url):
    for n, path in enumerate(paths, 1):
        m = re.match(r"HELP(\d\d)\.TXT$", os.path.basename(path))
        if not m or int(m.group(1)) != n:
            sys.exit("%s: expected HELP%02d.TXT" % (path, n))
    pages = []
    for n, path in enumerate(paths, 1):
        img, lines = page_image(path, url)
        for line in lines[:2] + lines[20:22]:   # the heading: on top, or under a lo-res page's graphics
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

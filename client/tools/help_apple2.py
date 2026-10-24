#!/usr/bin/env python3
"""The Apple II instruction pages (apple2_text) as one file of text-page images.

Usage:
    help_apple2.py OUT.HLP HELP01.TXT HELP02.TXT ...   check them, then write OUT
    help_apple2.py --check HELP01.TXT ...                only check

A page is plain ASCII: at most 24 lines of at most 40 characters, shown as
typed in normal video. Text between { and } shows in inverse (upper case,
digits and punctuation only: the character set has no inverse lower case).
Pages are numbered 01, 02... with no gaps, and a heading ending in n/m must
have n = the page number and m = the page count, as on the Atari
(help_disk.py).

Each page becomes 1024 bytes, an image of text page 2 ($0800) with its rows in
the screen's interleaved order; page n is at (n-1) * 1024 in OUT, which
src/apple2/help.s reads straight onto the screen."""
import os
import re
import sys

NORMAL, INV = 0x80, 0x00


def row_offset(r):
    return (r & 7) * 0x80 + (r >> 3) * 40


def page_image(path):
    lines = open(path).read().rstrip("\n").split("\n")
    if len(lines) > 24:
        sys.exit("%s: %d lines, at most 24" % (path, len(lines)))
    img = bytearray(1024)
    for r in range(24):
        img[row_offset(r):row_offset(r) + 40] = bytes([ord(" ") | NORMAL]) * 40
    for r, line in enumerate(lines):
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
    return bytes(img), lines


def check(paths):
    for n, path in enumerate(paths, 1):
        m = re.match(r"HELP(\d\d)\.TXT$", os.path.basename(path))
        if not m or int(m.group(1)) != n:
            sys.exit("%s: expected HELP%02d.TXT" % (path, n))
    pages = []
    for n, path in enumerate(paths, 1):
        img, lines = page_image(path)
        for line in lines[:2]:
            m = re.search(r"(\d+)/(\d+)\s*}?\s*$", line)
            if m and (int(m.group(1)), int(m.group(2))) != (n, len(paths)):
                sys.exit("%s: says %s/%s, is %d/%d" % (path, m.group(1), m.group(2), n, len(paths)))
        pages.append(img)
    return pages


def main():
    args = sys.argv[1:]
    if args and args[0] == "--check":
        check(args[1:])
        return
    out, paths = args[0], args[1:]
    pages = check(paths)
    with open(out, "wb") as f:
        f.write(b"".join(pages))
    print("%s: %d help pages" % (out, len(pages)))


if __name__ == "__main__":
    main()

#!/usr/bin/env python3
"""A GEOS file icon (24x21, as grc65's `icon` takes it: 63 bytes, 3 a line) from a picture in text, # set.

    geos_icon.py src/geos/icon.txt build/geos/icon.raw
"""
import sys


def main():
    rows = [r.rstrip("\n") for r in open(sys.argv[1]) if r.strip()]
    if len(rows) != 21 or any(len(r) != 24 for r in rows):
        raise SystemExit(f"{sys.argv[1]}: 21 lines of 24 wanted")
    out = bytearray()
    for r in rows:
        for b in range(3):
            v = 0
            for c in r[b * 8:b * 8 + 8]:
                v = v << 1 | (c == "#")
            out.append(v)
    open(sys.argv[2], "wb").write(out)


if __name__ == "__main__":
    main()

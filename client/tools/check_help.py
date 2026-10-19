#!/usr/bin/env python3
"""Check the instruction pages before they go on the disk.

    check_help.py help/HELP01.TXT help/HELP02.TXT ...

Each page is one screen of ATASCII: at most 23 lines (the bottom row is the
page bar), each at most 40 characters, ended by EOL ($9B). Pages must be
numbered from 01 with no gaps, since the Atari stops at the first missing one.
"""
import re
import sys
from pathlib import Path

ROWS, COLS, EOL = 23, 40, 0x9B


def check(path):
    data = path.read_bytes()
    if b"\n" in data and EOL not in data:
        return [f"{path.name}: has PC line endings, not ATASCII EOL ($9B)"]
    lines = data.split(bytes([EOL]))
    if lines and lines[-1] == b"":
        lines.pop()                         # the last line's EOL
    errs = []
    if len(lines) > ROWS:
        errs.append(f"{path.name}: {len(lines)} lines, max {ROWS}")
    for n, line in enumerate(lines, 1):
        if len(line) > COLS:
            errs.append(f"{path.name} line {n}: {len(line)} characters, max {COLS}")
    return errs


def main():
    paths = sorted(Path(p) for p in sys.argv[1:])
    if not paths:
        sys.exit("no help pages")
    errs = []
    for want, path in enumerate(paths, 1):
        m = re.fullmatch(r"HELP(\d\d)\.TXT", path.name)
        if not m or int(m.group(1)) != want:
            errs.append(f"{path.name}: expected HELP{want:02d}.TXT (pages are numbered 01, 02... with no gaps)")
        errs += check(path)
    if errs:
        sys.exit("\n".join(errs))
    print(f"help pages OK: {len(paths)}")


if __name__ == "__main__":
    main()

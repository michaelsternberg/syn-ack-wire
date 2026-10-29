#!/usr/bin/env python3
"""The H89's circuit board and title screen, as a C header for src/h89/display_text.c.

Usage:
    screens.py TITLE.atascii MARGINS.txt > screens.h

MARGINS.txt is the board either side of the 40-column game, as it shows, in UTF-8: 24 lines of at most 80
characters, columns 0-18 the left panel and 61-79 the right, columns 19-60 blank. It may use the box pieces
─ │ ┌ ┐ └ ┘ ┬ ┴ ├ ┤ ┼, ○ (a pad), ■ (a chip's body), ═ and ╪ (a bus) and ≡ (a connector's pins).

The title screen is the logo (TITLE.atascii, the Atari's, as tools/title_art.py reads it) as an outline with
each column doubled and centred on the screen, keeping the Atari's proportions on the H19's narrow cells (a piece with an arm to the
right goes on with a horizontal line), over the board. The board is cleared a cell round the logo: a chip
the clearing touches goes whole, a trace it cuts ends in a pad (a corner or tee loses the arm), and the
scraps left shorter than SCRAP cells, and pads with no trace, go.

Each cell becomes an H19 code in graphics mode (ESC F), bit 7 set for reverse video (ESC p): a pad is ●, a
chip's body a reverse space, the bus a reverse ─ (and ┼), the pins a reverse ▒. The header has
margins[24][38] (each row's left panel, then its right) and title_art[]: for each run of cells, its row,
column, length and cells, ending in 0xFF."""
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
import title_art  # noqa: E402

ROWS, LEFT, RIGHT, WIDTH = 24, 19, 61, 19
SCRAP = 5

H19 = dict(zip("─│┌┐└┘┬┴├┤┼▀○", "a`fcedsuvtbp^"))
H19_REV = {"■": " ", "═": "a", "╪": "b", "≡": "i"}

ARMS = {"─": "lr", "│": "ud", "┌": "dr", "┐": "dl", "└": "ur", "┘": "ul", "┬": "lrd", "┴": "lru",
        "├": "udr", "┤": "udl", "┼": "udlr", "═": "lr", "╪": "udlr"}
PIECE = {frozenset(v): k for k, v in ARMS.items() if k not in "═╪"}
STEP = {"u": (-1, 0, "d"), "d": (1, 0, "u"), "l": (0, -1, "r"), "r": (0, 1, "l")}

ATASCII = {0x20: " ", 0x7C: "│", 0x12: "─", 0x01: "├", 0x04: "┤", 0x11: "┌", 0x05: "┐", 0x1A: "└",
           0x03: "┘", 0x17: "┬", 0x18: "┴", 0x13: "┼", 0x0C: "▀", 0x0B: "▀"}
RIGHT_ARM = {"─", "├", "┌", "└", "┬", "┴", "┼"}


def code(ch, where):
    if ch == " ":
        return 0x20
    if ch in H19:
        return ord(H19[ch])
    if ch in H19_REV:
        return ord(H19_REV[ch]) | 0x80
    sys.exit("%s: %r has no H19 character" % (where, ch))


def load_board(path):
    lines = open(path, encoding="utf-8").read().rstrip("\n").split("\n")
    if len(lines) != ROWS:
        sys.exit("%s: %d lines, must be %d" % (path, len(lines), ROWS))
    board = []
    for r, line in enumerate(lines):
        if len(line) > 80:
            sys.exit("%s:%d: %d characters, at most 80" % (path, r + 1, len(line)))
        line = line.ljust(80)
        if line[LEFT:RIGHT].strip():
            sys.exit("%s:%d: columns 19-60 must be blank (the game and its gutters)" % (path, r + 1))
        for ch in line:
            code(ch, "%s:%d" % (path, r + 1))
        board.append(list(line))
    return board


def logo_rows(path):
    art = title_art.load(path)
    rows = []
    for line in art:
        bad = [b for b in line if b not in ATASCII]
        if bad:
            sys.exit("%s: ATASCII $%02X has no H19 character" % (path, bad[0]))
        rows.append("".join(ATASCII[b] + ("─" if ATASCII[b] in RIGHT_ARM else "▀" if ATASCII[b] == "▀" else " ")
                            for b in line).rstrip())
    # centred on the 80 columns: the Atari's art sits half a column left of its 40, a whole one doubled
    first = min(len(l) - len(l.lstrip()) for l in rows if l.strip())
    last = max(len(l) for l in rows)
    shift = (80 - first - last) // 2
    return [" " * shift + l if shift >= 0 else l[-shift:] for l in rows]


def links(board, r, c):
    ch = board[r][c]
    for a in (ARMS.get(ch, "") if ch != "○" else "udlr"):
        dr, dc, back = STEP[a]
        n = (r + dr, c + dc)
        if 0 <= n[0] < ROWS and 0 <= n[1] < 80:
            o = board[n[0]][n[1]]
            if (o == "○" and ch != "○") or back in ARMS.get(o, ""):
                yield n


def title(board, logo):
    board = [row[:] for row in board]
    clear = set()
    for r, line in enumerate(logo):
        if line.strip():
            a, b = len(line) - len(line.lstrip()), len(line)
            clear |= {(rr, c) for rr in (r - 1, r, r + 1) for c in range(a - 1, b + 1) if 0 <= rr < ROWS}
    for r, row in enumerate(board):             # chips go whole
        c = 0
        while c < 80:
            if row[c] in "■≡":
                e = c
                while e < 80 and row[e] in "■≡":
                    e += 1
                chip = {(rr, cc) for rr in (r - 1, r, r + 1) for cc in range(c - 1, e + 1)}
                if chip & clear:
                    clear |= chip
                c = e
            else:
                c += 1
    clear = {(r, c) for r, c in clear if 0 <= r < ROWS and 0 <= c < 80}
    cut = {(r, c) for r, c in clear if board[r][c] != " "}
    for r, c in clear:
        board[r][c] = " "
    for r in range(ROWS):                       # what the clearing cut loses its arm
        for c in range(80):
            ch = board[r][c]
            if ch in ARMS:
                arms = set(ARMS[ch])
                lost = {a for a in arms if (r + STEP[a][0], c + STEP[a][1]) in cut}
                if lost:
                    left = arms - lost
                    board[r][c] = "○" if len(left) == 1 else " " if not left else PIECE.get(frozenset(left), ch)
    near = {(r + dr, c + dc) for r, c in cut for dr, dc, _ in STEP.values()}
    seen = set()
    for start in sorted(near):                  # scraps
        r, c = start
        if not (0 <= r < ROWS and 0 <= c < 80) or start in seen or (board[r][c] not in ARMS and board[r][c] != "○"):
            continue
        part, todo = set(), [start]
        while todo:
            n = todo.pop()
            if n not in part:
                part.add(n)
                todo += list(links(board, *n))
        seen |= part
        if len(part) < SCRAP:
            for pr, pc in part:
                board[pr][pc] = " "
    for r in range(ROWS):                       # lone pads
        for c in range(80):
            if board[r][c] == "○" and not any(links(board, r, c)):
                board[r][c] = " "
    for r, line in enumerate(logo):
        for c, ch in enumerate(line):
            if ch != " ":
                board[r][c] = ch
    return board


def runs(row):
    """(column, cells) for each stretch of the row, a gap of more than 3 blanks ending it."""
    out, c = [], 0
    while c < 80:
        if row[c] == " ":
            c += 1
            continue
        e = c
        while e < 80 and (row[e] != " " or any(ch != " " for ch in row[e:e + 4])):
            e += 1
        while row[e - 1] == " ":
            e -= 1
        out.append((c, row[c:e]))
        c = e
    return out


def main():
    if len(sys.argv) != 3:
        sys.exit(__doc__)
    board = load_board(sys.argv[2])
    logo = logo_rows(sys.argv[1])
    if len(logo) > 14:
        sys.exit("%s: the logo has %d rows, at most 14 (rows 0-13: the title's lines start at row 15)" % (sys.argv[1], len(logo)))
    screen = title(board, logo)
    print("/* Generated by tools/h89/screens.py from %s and %s: don't edit. */" % tuple(sys.argv[1:]))
    print("static const unsigned char margins[%d][%d] = {     /* each row: the left panel, then the right */"
          % (ROWS, 2 * WIDTH))
    for r, row in enumerate(board):
        cells = [code(ch, "row %d" % r) for ch in row[:WIDTH] + row[RIGHT:]]
        print("    { " + ", ".join("0x%02X" % b for b in cells) + " },")
    print("};")
    print("#define MARGIN_W %d                  /* each panel's width; the right one starts at column %d */"
          % (WIDTH, RIGHT))
    print("static const unsigned char title_art[] = {   /* row, column, length, cells */")
    for r, row in enumerate(screen):
        for c, cells in runs(row):
            print("    %d, %d, %d, %s," % (r, c, len(cells), ", ".join("0x%02X" % code(ch, "title") for ch in cells)))
    print("    0xFF\n};")
    print("#define TITLE_END %d          /* the logo's last row */" % (len(logo) - 1))


if __name__ == "__main__":
    main()

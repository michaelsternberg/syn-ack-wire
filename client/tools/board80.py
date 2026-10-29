"""The 80-column title screen shared by the H89 (tools/h89/screens.py) and MS-DOS (tools/margins_msdos.py): the
Atari's logo (title4.atascii) as an outline with each column doubled, keeping the Atari's proportions on an
80-column screen's narrow cells, over the circuit board either side of the game.

A piece with an arm to the right goes on with a horizontal line, the hyphen's ▀ with another ▀, a cell just
before the hyphen with ▀ too (the line before it then sits half a cell from it, as the line after it does: the
hyphen centred between them, as on the Atari), anything else with a space; the logo is centred on the 80 columns. The board is cleared a cell round the logo: a chip the
clearing touches goes whole, a trace it cuts ends in a pad (a corner or tee loses the arm), and the scraps left
shorter than SCRAP cells, and pads with no trace, go.

A board is a list of rows of 80 one-character strings in UTF-8's box pieces ─ │ ┌ ┐ └ ┘ ┬ ┴ ├ ┤ ┼, ○ (a pad),
■ (a chip's body), ═ and ╪ (a bus) and ≡ (a connector's pins)."""
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import title_art  # noqa: E402

SCRAP = 5

ARMS = {"─": "lr", "│": "ud", "┌": "dr", "┐": "dl", "└": "ur", "┘": "ul", "┬": "lrd", "┴": "lru",
        "├": "udr", "┤": "udl", "┼": "udlr", "═": "lr", "╪": "udlr"}
PIECE = {frozenset(v): k for k, v in ARMS.items() if k not in "═╪"}
STEP = {"u": (-1, 0, "d"), "d": (1, 0, "u"), "l": (0, -1, "r"), "r": (0, 1, "l")}

ATASCII = {0x20: " ", 0x7C: "│", 0x12: "─", 0x01: "├", 0x04: "┤", 0x11: "┌", 0x05: "┐", 0x1A: "└",
           0x03: "┘", 0x17: "┬", 0x18: "┴", 0x13: "┼", 0x0C: "▀", 0x0B: "▀"}
RIGHT_ARM = {"─", "├", "┌", "└", "┬", "┴", "┼"}


def double(ch, after):
    """A logo cell and the one after it, 80 columns wide; `after` is the logo's next cell."""
    return ch + ("─" if ch in RIGHT_ARM else "▀" if "▀" in (ch, after) else " ")


def logo_shift(path):
    """The logo's rows doubled, unshifted, and the shift that centres them on the 80 columns."""
    art = title_art.load(path)
    rows = []
    for line in art:
        bad = [b for b in line if b not in ATASCII]
        if bad:
            sys.exit("%s: ATASCII $%02X has no box piece" % (path, bad[0]))
        cells = [ATASCII[b] for b in line] + [" "]
        rows.append("".join(double(ch, after) for ch, after in zip(cells, cells[1:])).rstrip())
    # the Atari's art sits half a column left of its 40, a whole one doubled
    first = min(len(l) - len(l.lstrip()) for l in rows if l.strip())
    last = max(len(l) for l in rows)
    return rows, (80 - first - last) // 2


def logo_rows(path):
    """The logo's rows doubled and centred on the 80 columns."""
    rows, shift = logo_shift(path)
    return [" " * shift + l if shift >= 0 else l[-shift:] for l in rows]


def links(board, r, c):
    ch = board[r][c]
    for a in (ARMS.get(ch, "") if ch != "○" else "udlr"):
        dr, dc, back = STEP[a]
        n = (r + dr, c + dc)
        if 0 <= n[0] < len(board) and 0 <= n[1] < 80:
            o = board[n[0]][n[1]]
            if (o == "○" and ch != "○") or back in ARMS.get(o, ""):
                yield n


def cleared(board, logo):
    """The board with the room the logo needs cleared round it (the logo not drawn)."""
    rows = len(board)
    board = [row[:] for row in board]
    clear = set()
    for r, line in enumerate(logo):
        if line.strip():
            a, b = len(line) - len(line.lstrip()), len(line)
            clear |= {(rr, c) for rr in (r - 1, r, r + 1) for c in range(a - 1, b + 1) if 0 <= rr < rows}
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
    clear = {(r, c) for r, c in clear if 0 <= r < rows and 0 <= c < 80}
    cut = {(r, c) for r, c in clear if board[r][c] != " "}
    for r, c in clear:
        board[r][c] = " "
    for r in range(rows):                       # what the clearing cut loses its arm
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
        if not (0 <= r < rows and 0 <= c < 80) or start in seen or (board[r][c] not in ARMS and board[r][c] != "○"):
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
    for r in range(rows):                       # lone pads
        for c in range(80):
            if board[r][c] == "○" and not any(links(board, r, c)):
                board[r][c] = " "
    return board


def title(board, logo):
    """The title screen: the cleared board with the logo on it."""
    board = cleared(board, logo)
    for r, line in enumerate(logo):
        for c, ch in enumerate(line):
            if ch != " ":
                board[r][c] = ch
    return board

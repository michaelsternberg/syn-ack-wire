#!/usr/bin/env python3
"""Paint the Amiga client's title picture (320 x 256, 32 colours) and write it as src/amiga/title_art.c.

The picture is drawn straight into palette indices, with a 4x4 ordered dither between neighbouring pens of a ramp, as
a 90s Amiga artist would have in Deluxe Paint. The pens the title's buttons and text use keep the game's colours (the
palette of src/amiga/palette.h); the free ones (8-14, the logo's colour-cycling ramp, and 20-26, the sky) are the
picture's own. Run:  tools/amiga_title_art.py  [preview.png]   (needs Pillow and numpy; the output is committed, since
the font it paints the logo with differs from machine to machine.)"""
import os, re, sys, tomllib
import numpy as np
from PIL import Image, ImageDraw, ImageFont

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.join(HERE, "..", "..")
W, H = 320, 256
FONT = "/usr/share/fonts/gsfonts/NimbusSansNarrow-BoldOblique.otf"

TITLE = tomllib.load(open(os.path.join(ROOT, "theme", "theme.toml"), "rb"))["game"]["title"]

# the game's palette, read from the generated header so the two cannot differ
pal = [int(m, 16) for m in re.findall(r"0x([0-9A-F]{4}),", open(os.path.join(HERE, "..", "src", "amiga", "palette.h")).read())]
assert len(pal) == 32
FREE = {                                # the picture's own colours (0x0RGB)
    8: 0x0012, 9: 0x0035, 10: 0x008B, 11: 0x00DF, 12: 0x008B, 13: 0x0035, 14: 0x0012,      # the logo's rim glow: a light that runs round it
    20: 0x0DFF, 21: 0x09CF, 22: 0x058D, 23: 0x0247,                                       # chrome, the sky half: bright to dark
    24: 0x0A62, 25: 0x0D94, 26: 0x0FB6,                                                   # chrome, the ground half: brown to gold
}
pal = [FREE.get(i, c) for i, c in enumerate(pal)]
BG0, BG1, BG2, EDGE, EDGEHI, WHITE, DIM, STEEL, CYAN, AMBER, RED, MAG, TEXT = 0, 4, 5, 6, 7, 3, 2, 29, 15, 16, 27, 31, 1
RAMP = list(range(8, 15))
RAIN = [BG0, BG1, BG2, EDGE, EDGEHI, STEEL, CYAN, WHITE]

BAYER = np.array([[0, 8, 2, 10], [12, 4, 14, 6], [3, 11, 1, 9], [15, 7, 13, 5]]) / 16.0
img = np.zeros((H, W), np.uint8)
ys, xs = np.mgrid[0:H, 0:W]


def ramp_pick(t, ramp):
    """t in 0..1 over the ramp -> a pen, dithered between the two nearest."""
    f = np.clip(t, 0, 0.9999) * (len(ramp) - 1)
    lo = f.astype(int)
    frac = f - lo
    hi = np.minimum(lo + 1, len(ramp) - 1)
    ra = np.array(ramp)
    return np.where(frac > BAYER[ys[:t.shape[0], :t.shape[1]] % 4, xs[:t.shape[0], :t.shape[1]] % 4], ra[hi], ra[lo])


rng = np.random.RandomState(1990)

# the rain: columns of falling light, behind the logo (which is the shared title art, drawn below)
SKY_END = 88
img[:SKY_END] = BG0
for x in range(0, W, 3):
    if rng.rand() < 0.8:
        head = rng.randint(6, SKY_END + 18)
        ln = rng.randint(8, 34)
        for k in range(ln):
            y = head - k
            if 0 <= y < SKY_END:
                lvl = 7 if k == 0 else max(1, 6 - k * 6 // ln)
                img[y, x] = RAIN[lvl]
                if k == 0 and x + 1 < W:
                    img[y, x + 1] = RAIN[3]
for _ in range(120):                                  # stray specks in the dark between the columns
    x, y = rng.randint(0, W), rng.randint(0, SKY_END)
    if img[y, x] == BG0:
        img[y, x] = [BG1, BG2][rng.randint(0, 2)]
img[SKY_END] = EDGE
img[SKY_END + 1] = EDGEHI
for x in range(0, W, 8):
    img[SKY_END + 2, x] = CYAN if x % 64 == 0 else EDGE

# below the logo: a board of circuit traces (the panels and buttons cover most of it), and stray stars of light
img[SKY_END + 3:190] = BG0
for y in range(96, 190, 16):
    for x in range(8, W, 16):
        img[y, x] = BG2
for _ in range(34):                                   # a trace walks in straight runs and 45 degree bends and ends in a pad
    x, y = int(rng.randint(0, W)), int(rng.randint(SKY_END + 4, 188))
    dx, dy = [(1, 0), (-1, 0), (0, 1), (0, -1)][rng.randint(0, 4)]
    for seg in range(rng.randint(3, 7)):
        for _ in range(rng.randint(6, 24)):
            if 0 <= x < W and SKY_END + 3 <= y < 190:
                img[y, x] = EDGE
            x += dx; y += dy
        dx, dy = [(dx, dy), (dy, dx), (-dy, -dx), (dx + dy, dy - dx) if dx and not dy else (dx, dy)][rng.randint(0, 3)]
        dx, dy = max(-1, min(1, dx)), max(-1, min(1, dy))
    if 1 <= x < W - 1 and SKY_END + 4 <= y < 189:
        img[y - 1:y + 2, x - 1:x + 2] = CYAN
        img[y, x] = BG0
for x in range(0, 24):                                # the board's edge connectors, left and right
    for k in range(0, 100, 6):
        img[96 + k, x] = EDGE if x < 18 else EDGEHI
        img[96 + k, W - 1 - x] = EDGE if x < 18 else EDGEHI

# PAL's bottom band: a wireframe globe on the horizon over a grid floor
HOR = 208
img[190:HOR] = BG0
for _ in range(30):
    x, y = rng.randint(0, W), rng.randint(191, HOR - 2)
    img[y, x] = [BG2, EDGE, DIM, STEEL][rng.randint(0, 4)]
img[HOR - 7:HOR] = ramp_pick(np.broadcast_to(((np.arange(7)[:, None] + 1) / 7.0), (7, W)), RAIN)
img[HOR:H] = BG1
GR, GCX, GCY = 19, 160, HOR + 1
rr2 = (xs - GCX) ** 2 + (ys - GCY) ** 2
globe = (rr2 <= GR * GR) & (ys < HOR)
img[globe] = BG0
for k in range(-3, 4):                                # meridians: ellipses of shrinking width
    ex = GR * abs(k) / 4.0
    if k == 0:
        img[globe & (np.abs(xs - GCX) < 1)] = CYAN
    else:
        ell = np.abs(((xs - GCX) / max(ex, 1)) ** 2 + ((ys - GCY) / float(GR)) ** 2 - 1) < 0.08
        img[globe & ell] = EDGEHI
for lat in (6, 12, 18):                               # parallels
    img[globe & (np.abs(ys - (HOR - lat)) < 1)] = EDGE
img[(np.abs(rr2 - GR * GR) < GR * 2) & (ys < HOR)] = CYAN        # the rim
for i in range(6):                                    # floor lines, closer together towards the horizon
    y = HOR + int((i / 6.0) ** 2 * (H - HOR))
    img[y, :] = CYAN if i < 3 else EDGEHI
for j in range(-7, 8):                                # and the lines that run to the horizon
    for y in range(HOR, H):
        x = int(160 + j * 56 * (y - HOR + 1) / (H - HOR))
        if 0 <= x < W:
            img[y, x] = CYAN
img[HOR, :] = CYAN

# HUD corner brackets round the whole picture
for (bx, by, sx, sy) in ((1, 1, 1, 1), (W - 2, 1, -1, 1), (1, H - 2, 1, -1), (W - 2, H - 2, -1, -1)):
    for k in range(12):
        img[by, bx + sx * k] = CYAN
        img[by + sy * k, bx] = CYAN

# the logo: the title art every platform shares (src/atari/title4.atascii: boxed outlines whose letters are the enclosed gaps),
# drawn large. The walls are thin steel lines; the letters are the strokes, filled with chrome (a sky-blue gradient over a dark
# horizon, then gold, for each of the two rows), bevelled, with glare and sparkles, in a rim of light that colour cycling turns.
sys.path.insert(0, HERE)
import title_art as shared

ART = os.path.join(HERE, "..", "src", "atari", "title4.atascii")
art = shared.load(ART)
stroke = shared.strokes(art)
CW, CH = 9, 6
cols = [c for line in art for c, b in enumerate(line) if b != 0x20]
c_lo, c_hi = min(cols), max(cols)
X0 = (W - (c_hi - c_lo + 1) * CW) // 2 - c_lo * CW
Y0 = 3
mask = np.zeros((H, W), bool)
walls = np.zeros((H, W), bool)
bands = [(r.start, r.stop) for r in shared.bands(art)]
for r, c in stroke:
    mask[Y0 + r * CH:Y0 + (r + 1) * CH, X0 + c * CW:X0 + (c + 1) * CW] = True
UP, DOWN, LEFT, RIGHT = 1, 2, 4, 8
SIDES = {0x7C: UP | DOWN, 0x12: LEFT | RIGHT, 0x11: DOWN | RIGHT, 0x05: DOWN | LEFT, 0x1A: UP | RIGHT, 0x03: UP | LEFT,
         0x17: LEFT | RIGHT | DOWN, 0x18: LEFT | RIGHT | UP, 0x04: UP | DOWN | LEFT, 0x01: UP | DOWN | RIGHT, 0x13: UP | DOWN | LEFT | RIGHT}
for r, line in enumerate(art):
    for c, b in enumerate(line):
        x, y = X0 + c * CW, Y0 + r * CH
        if b in (0x0B, 0x0C):                           # the hyphen: a bar of quarter blocks
            mask[y:y + CH // 2 + 1, x:x + CW] = True
        elif b in SIDES:
            m = SIDES[b]
            cx, cy = x + CW // 2, y + CH // 2
            if m & UP: walls[y:cy + 1, cx - 1:cx + 1] = True
            if m & DOWN: walls[cy:y + CH, cx - 1:cx + 1] = True
            if m & LEFT: walls[cy - 1:cy + 1, x:cx + 1] = True
            if m & RIGHT: walls[cy - 1:cy + 1, cx:x + CW] = True


def grow(a, n):
    out = a.copy()
    for _ in range(n):
        o = out.copy()
        o[1:] |= out[:-1]; o[:-1] |= out[1:]; o[:, 1:] |= out[:, :-1]; o[:, :-1] |= out[:, 1:]
        o[1:, 1:] |= out[:-1, :-1]; o[:-1, :-1] |= out[1:, 1:]; o[1:, :-1] |= out[:-1, 1:]; o[:-1, 1:] |= out[1:, :-1]
        out = o
    return out


rim = grow(mask, 3) & ~grow(mask, 1)
img[rim] = np.array(RAMP)[((xs + ys) // 5) % 7][rim]  # the rim of light: diagonal bands of the glow ramp, colour-cycled at run time
img[walls] = TEXT                                      # the walls: thin pale lines
img[np.roll(walls, 1, axis=0) & ~walls & ~mask] = DIM  # and a shade under them
img[grow(mask, 1) & ~mask] = BG0                       # a dark edge round each letter

t = np.zeros((H, W))                                   # how far down its own row of letters each pixel is
for (r0, r1) in bands:
    top, bot = Y0 + r0 * CH, Y0 + r1 * CH
    t[top:bot] = ((ys[top:bot] - top) / float(bot - top))
HORIZON = 0.55
sky = ramp_pick(np.clip(t / HORIZON, 0, 1), [WHITE, 20, 21, 22, 23])
ground = ramp_pick(np.clip((t - HORIZON - 0.05) / (1 - HORIZON - 0.05), 0, 1), [24, 25, 26, TEXT])
chrome = np.where(t < HORIZON, sky, np.where(t < HORIZON + 0.05, BG1, ground))
img[mask] = chrome[mask]
snow = mask & (np.repeat(rng.rand(H, W // 2), 2, axis=1) < 0.30)   # TV snow in the letters, as on the other platforms: the glow ramp's
img[snow] = np.array(RAMP)[np.repeat(rng.randint(0, 7, (H, W // 2)), 2, axis=1)][snow]   # pens, so colour cycling makes it twinkle
glare = mask & (t < HORIZON) & (np.abs((xs + ys * 2) % 96 - 30) < 3)
img[glare] = WHITE
light = mask & (~np.roll(mask, 1, axis=0) | ~np.roll(mask, 1, axis=1))                    # top and left edges
shade = mask & (~np.roll(mask, -1, axis=0) | ~np.roll(mask, -1, axis=1))                  # bottom and right edges
img[light] = WHITE
img[shade & ~light] = BG1
cand = np.argwhere(mask[:Y0 + 4 * CH] & ~np.roll(mask, 1, axis=0)[:Y0 + 4 * CH])           # sparkles: four-point stars on top edges
cand = cand[np.argsort(cand[:, 1])]
for q in (len(cand) // 6, len(cand) // 2, len(cand) * 5 // 6):
    sy, sx = cand[q]
    sy += 1
    img[max(sy - 3, 0):sy + 4, sx] = WHITE
    img[sy, sx - 3:sx + 4] = WHITE
    img[sy - 1:sy + 2, sx - 1:sx + 2] = CYAN
    img[sy, sx] = WHITE

if len(sys.argv) > 1:
    rgb = np.array([[(c >> 8 & 15) * 17, (c >> 4 & 15) * 17, (c & 15) * 17] for c in pal], np.uint8)
    Image.fromarray(rgb[img]).resize((W * 3, H * 3), Image.NEAREST).save(sys.argv[1])

# planar, 5 planes of 40 bytes a row, then packed: a run of the same byte is (257 - n, byte), literals are (n - 1, bytes)
planes = []
for p in range(5):
    bits = ((img >> p) & 1).astype(np.uint8)
    planes.append(np.packbits(bits, axis=1).tobytes())
raw = b"".join(planes)


def pack(data):
    out = bytearray()
    i = 0
    n = len(data)
    while i < n:
        run = 1
        while i + run < n and data[i + run] == data[i] and run < 128:
            run += 1
        if run >= 3:
            out += bytes([257 - run, data[i]])
            i += run
        else:
            j = i
            while j < n and j - i < 128 and not (j + 2 < n and data[j] == data[j + 1] == data[j + 2]):
                j += 1
            if j == i:
                j = i + 1
            out += bytes([j - i - 1]) + data[i:j]
            i = j
    return bytes(out)


packed = pack(raw)
dest = os.path.join(HERE, "..", "src", "amiga", "title_art.c")
with open(dest, "w") as o:
    o.write("/* GENERATED by tools/amiga_title_art.py: the title picture, 320 x 256 in 5 planes, packed (see gfx_art). Do not edit. */\n")
    o.write('#include "title_art.h"\n\n')
    o.write("const unsigned short title_art_palette[32] = {\n")
    for i in range(0, 32, 8):
        o.write("    " + ", ".join("0x%04X" % c for c in pal[i:i + 8]) + ",\n")
    o.write("};\n\n")
    o.write("const unsigned char title_art[%d] = {\n" % len(packed))
    for i in range(0, len(packed), 20):
        o.write("    " + ",".join(str(b) for b in packed[i:i + 20]) + ",\n")
    o.write("};\n\nconst unsigned short title_art_len = %d;\n" % len(packed))
print("title_art.c: %d bytes packed from %d" % (len(packed), len(raw)))
